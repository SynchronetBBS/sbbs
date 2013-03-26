/*	kissAX25lib.js for Synchronet 3.16+
	VE3XEC (echicken -at- bbs.electronicchicken.com)
	
This library adds support for the KISS and AX.25 protocols to Synchronet BBS.
You can find the latest copy on the Synchronet CVS at cvs.synchro.net.
	
Dependencies:
	
	exec/load/ax25defs.js	- Masks for bitwise operations
	ctrl/kiss.ini			- This is where your TNCs are defined and configured
	
Objects, Properties, and Methods:
	
AX25 (Object)
	
	Contains settings that should be global to your entire application.  Is also
	the parent of all other KISS & AX.25 related objects.
	
	Properties:
	
	logging	- Set to 'true' to log all packets in and out of all TNCs
			  (This logging will be done at the DEBUG log level.)
	tncs	- Object containing references to every loaded AX25.KISSTNC object
	clients	- Object containing references to every loaded AX25.Client object
	
	Methods:
	
	loadTNC(tncName)
	
		Where 'tncName' is a section name from ctrl/kiss.ini, returns an
		AX25.KISSTNC object, and adds that object to AX25.tncs
	
	AX25.loadAllTNCs()

		Populates AX25.tncs with an object for each section in ctrl/kiss.ini.	
	
AX25.KISSTNC(serialPort, baudRate, callsign, ssid)
	
	Creates a new AX25.KISSTNC object, eg:
	var k = new AX25.KISSTNC("COM1", 9600, "MYCALL", 1);
	A reference to this TNC will be placed in AX25.tncs.
	
	Arguments:
	
	serialPort	- Device name; "COM1", "/dev/ttyUSB0", etc.
	baudRate	- The speed of the serial connection between your computer
				  and your TNC.  1200, 9600, etc.
	callsign	- Alphanumerics only
	ssid		- A number between 0 and 15
	name		- Whatver you want; usually a section name from kiss.ini
	
	Settings:
	
	serialPort	- As above
	baudRate	- As above
	callsign	- As above
	ssid		- As above
	timeout		- Timeout when reading a packet from the TNC, in milliseconds
	txDelay		- The delay between the time when the TNC keys down the
				  transmitter and when it sends audio, in milliseconds
	txTail		- The delay between the time when the TNC stops sending
				  audio and when it unkeys the transmitter, in milliseconds
	persistence	- CSMA persistence, between 0 and 1 (default: 0.25)
	slotTime	- CSMA slot time, in milliseconds
	fullDuplex	- true/false (default: false)
	
	Properties:
	
	dataWaiting	- true if a packet is waiting to be read
	dataPending	- true if a packet is waiting to be sent
	
	Methods:
	
	setHardware(command)	- Set a TNC-specific configuration option, where
							  'command' is a number from 0 to 255.  Refer to
							  your TNC documentation for valid 'command' values
	
	exitKISS()				- Take the TNC out of KISS mode
	
	receive()				- Returns an AX25.Packet object, or false if there
							  are no packets waiting in the receive buffer
	
	send(packet)			- Adds AX25.Packet object 'packet' to the outgoing
							  packet queue
							  
	cycle()					- Appends to the receive buffer any packets waiting
							  to be read from the interface.  Sends to the
							  interface any packets waiting in the outgoing
							  queue.
							  
AX25.Packet(frame)

	Creates a new AX25.Packet object.

	Arguments:
	
	frame	- Optional, and normally only supplied by kissTNC.getPacket(),
			  which returns an AX25.Packet object to you.
			  
	Properties:
	
	destinationCallsign	- The callsign of the destination station (<= 6 chars)
	destinationSSID		- The SSID of the destination station (0 - 15)
	sourceCallsign		- The callsign of the source station (<= 6 chars)
	sourceSSID			- The SSID of the source station (0 - 15)
	repeaterPath		- An array of { callsign, ssid } objects
	pollFinal			- True if the poll/final bit is set
	command				- True if this is a command
	response			- True if this is a response
	control				- The control field (frame type with N(R), N(S), P/F
						  set as applicable.) This property is read only. If
						  you want to modify it, modify its constituent parts:
						  .type, .nr, .ns, .pollFinal
	type				- The control field with N(R), N(S), P/F left unset
						  (Will match one of the frame types defined in
						  ax25defs.js, eg. U_FRAME.)
	nr					- The receive sequence number of an I or S frame
	ns					- The send sequence number of an I frame
	pid					- The PID field of an I or UI frame
	info				- The info field of an I or UI frame
	clientID			- If there was a client associated with this packet,
						  this is what its ID would be.
	
	Methods:
	
	These methods are normally called indirectly by the kissTNC object's
	getPacket() and sendPacket() methods.
	
	disassemble(frame)	- Where frame is an array of the bytes of a raw AX.25
						  frame (less the start/stop flags and FCS), populates
						  the above properties of this instance of the object
						  with data from that frame. (If 'frame' is supplied
						  when creating an AX25.Packet object, this method will
						  be called automatically.)
						  
	assemble()			- Opposite of disassemble; returns an array of bytes
						  of a raw AX.25 frame (less the start/stop flags and
						  FCS) based on the properties of this instance of the
						  object. 
						  
AX25.Client(tnc, packet)

	Where 'tnc' is the AX25.KISSTNC object associated with this client, and the
	optional 'packet' argument is a *received* AX25.Packet object.

	Public Properties:
	
	callsign				- The callsign of the remote station
	ssid					- The SSID of the remote station
	id						- Unique ID for this client (can be compared against
							  the 'clientID' property of an inbound packet to
							  match an AX25.Packet with an existing AX25.Client.
	connected				- true if the link is active
	dataWaiting				- true if data is waiting in the receive buffer
	
	Public Methods:
	
	connect(callsign, ssid)	- Connect to station <callsign>-<ssid>, SSID will
							  default to 0 if omitted.
	disconnect()			- Terminate the link.
	receivePacket(packet)	- Add 'packet' (ie. a packet object read from an
							  AX25.KISSTNC object) to the incoming packet buffer
							  to be processed (and responded to) the next time
							  .cycle() is called.
	send(array)				- Add 'array' to the send buffer.  Data from this
							  buffer will be sent out in sequence when .cycle()
							  is called.
	sendString(string)		- Converts 'string' to an array of ASCII codes, then
							  places that array in the send buffer to be sent
							  out the next time .cycle() is called.
	receive()				- Returns an array of eight bit binary integers, or
							  false if no data is waiting to be read.
	receiveString()			- Treats the next array of bytes in the receive
							  buffer as an array of ASCII codes, converts that
							  array into a string and returns it.
	cycle()					- Sends out any pending data up to the maximum
							  amount possible.  Processes any packets in the
							  incoming packet buffer, populating the receive
							  buffer with data as applicable.	*/

load("ax25defs.js");

function logByte(b) {
	log(
		format(
			"%d%d%d%d%d%d%d%d",
			(b & (1<<7)) ? 1 : 0,
			(b & (1<<6)) ? 1 : 0,
			(b & (1<<5)) ? 1 : 0,
			(b & (1<<4)) ? 1 : 0,
			(b & (1<<3)) ? 1 : 0,
			(b & (1<<2)) ? 1 : 0,
			(b & (1<<1)) ? 1 : 0,
			(b & (1<<0)) ? 1 : 0
		)
	);
}

/*	distanceBetween(leader, follower, modulus)
	Find the difference between 'leader' and 'follower' modulo 'modulus'. */
var distanceBetween = function(l, f, m) {
	return (l < f) ? l + (m - f) : l - f;
}

/*	wrapAround(number, modulus)
	If 'number' happens to be negative, it will be wrapped back around 'modulus'
	eg. wrapAround(-1, 8) == 7. */
var wrapAround = function(n, m) {
	return (n < 0) ? m - Math.abs(n) : n;
}

/*	testCallsign(callsign) - boolean
	Returns true if 'callsign' is a valid AX.25 callsign (a string
	containing up to six letters and numbers only.) */
var testCallsign = function(callsign) {
	if(typeof callsign == "undefined" || callsign.length > 6)
		return false;
	callsign = callsign.toUpperCase().replace(/\s*$/g, "");
	for(var c = 0; c < callsign.length; c++) {
		var a = ascii(callsign[c]);
		if(
			(a >= 48 && a <= 57)
			||
			(a >=65 && a <=90)
		) {
			continue;
		}
		return false;
	}
	return true;
}

// Turns a string into an array of ASCII character codes
function stringToByteArray(s) {
	s = s.split("");
	var r = new Array();
	for(var i = 0; i < s.length; i++)
		r.push(ascii(s[i]));
	return r;
}

// Turns an array of ASCII character codes into a string
function byteArrayToString(s) {
	var r = "";
	for(var i = 0; i < s.length; i++)
		r += ascii(s[i]);
	return r;
}

var AX25 = {

	'logging'		: false,// Set true to log packets in and out of TNCs
	'tncs'			: {}, 	// Indexed by AX25.KISSTNC.id
	'clients'		: {}, 	// Indexed by AX25.Client.id/AX25.Packet.clientID

	// I plan to make this setting more configurable at some point.
	// Flag + Destination + Source + Repeaters + Control + PID + Info + Flag
	'maximumPacketSize' : 8 + 56 + 56 + 448 + 8 + 8 + 2048 + 8 // 2640 Bits
	
};

AX25.loadTNC = function(tncName) {
	var f = new File(system.ctrl_dir + "kiss.ini");
	f.open("r");
	var tncIni = f.iniGetObject(tncName);
	f.close();
	if(tncIni == null) {
		throw format(
			"AX25: KISS TNC %s not found in %skiss.ini",
			tncName, system.ctrl_dir
		);
	}
	var tnc = new AX25.KISSTNC(
		tncIni.serialPort,
		tncIni.baudRate,
		tncIni.callsign,
		tncIni.ssid
	);
	for(var property in tncIni) {
		if(
			property == "serialPort"
			||
			property == "baudRate"
			||
			property == "callsign"
			||
			property == "ssid"
		) {
			continue;
		}
		if(tnc.hasOwnProperty(property))
			tnc[property] = Number(tncIni[property]);
		else
			throw "AX25: Unknown TNC property " + property;
	}
	return tnc;
}

AX25.loadAllTNCs = function() {
	var f = new File(system.ctrl_dir + "kiss.ini");
	f.open("r");
	var sections = f.iniGetSections();
	f.close();
	if(typeof sections == "undefined" || sections == null) {
		throw format(
			"AX.25: Unable to read TNC configuration from %skiss.ini",
			system.ctrl_dir
		);
	}
	for(var s = 0; s < sections.length; s++)
		AX25.loadTNC(sections[s]);
}

AX25.KISSTNC = function(serialPort, baudRate, callsign, ssid) {
	
	var settings = {
		'serialPort'	: "",
		'baudRate'		: 0,
		'callsign'		: "",
		'ssid'			: 0,
		'port'			: 0,
		'timeout'		: 10000,
		'txDelay'		: 500,
		'persistence'	: 63,
		'slotTime'		: 100,
		'txTail'		: 0,
		'fullDuplex'	: false
	};
	
	var buffers = {
		'send' 		: [],
		'receive'	: []
	};
	
	var com = new COM("");
	
	var sendCommand = function(command, value) {
		com.sendBin(KISS_FEND, 1);
		com.sendBin((((settings.port<<4)|command)<<8)|value, 2);
		com.sendBin(KISS_FEND, 1);
	}
	
	this.__defineGetter__(
		"serialPort",
		function() {
			return settings.serialPort;
		}
	);
	
	this.__defineSetter__(
		"serialPort",
		function(serialPort) {
			if(com.is_open)
				com.close();
			if(typeof serialPort == "undefined")
				throw "Unable to open nonexistent serial port.";
			com = new COM(serialPort);
			com.open();
			if(!com.is_open) {
				throw format(
					"Error opening serial port '%s': %s.",
					serialPort,
					com.last_error
				);
			}
		}
	);
	
	this.__defineGetter__(
		"baudRate",
		function() {
			return settings.baudRate;
		}
	);
	
	this.__defineSetter__(
		"baudRate",
		function(baudRate) {
			if(typeof baudRate == "undefined" || isNaN(Math.abs(baudRate))) {
				throw format(
					"Invalid baud rate: %s.",
					baudRate
				);
			} else {
				settings.baudRate = Math.abs(baudRate);
				if(com.is_open)
					com.close();
				com.baud_rate = settings.baudRate;
				com.open();
				if(!com.is_open) {
					throw format(
						"Error setting baud rate '%s': %s",
						baudRate,
						com.last_error
					);
				}
			}
		}
	);
	
	this.__defineGetter__(
		"callsign",
		function() {
			return callsign;
		}
	);
	
	this.__defineSetter__(
		"callsign",
		function(callsign) {
			if(typeof callsign == "undefined" || !testCallsign(callsign)) {
				throw format(
					"Invalid callsign: %s.",
					callsign
				);
			} else {
				settings.callsign = callsign;
			}
		}
	);
	
	this.__defineGetter__(
		"ssid",
		function() {
			return settings.ssid;
		}
	);
	
	this.__defineSetter__(
		"ssid",
		function(ssid) {
			if(typeof ssid == "undefined" || isNaN(ssid))
				throw "Invalid SSID.";
			var ssid = Math.abs(ssid);
			if(ssid > 15) {
				throw format(
					"Invalid SSID: %s",
					ssid
				);
			}
			settings.ssid = ssid;
		}
	);
	
	this.__defineGetter__(
		"port",
		function() {
			return settings.port;
		}
	);
	
	this.__defineSetter__(
		"port",
		function(port) {
			if(typeof port == "undefined" || isNaN(port) || port < 0 || port > 15)
				throw "AX25.KISSTNC: Invalid TNC radio port assignment";
			settings.port = port;
		}
	);
	
	this.__defineGetter__(
		"id",
		function() {
			return md5_calc(
				format(
					"%s%s%s",
					this.callsign,
					this.ssid,
					this.serialPort,
					this.port
				),
				true
			);
		}
	);

	this.__defineGetter__(
		"timeout",
		function() {
			return settings.timeout;
		}
	);
	
	this.__defineSetter__(
		"timeout",
		function(timeout) {
			if(typeof timeout == "undefined" || isNaN(timeout))
				throw "Invalid timeout argument.";
			settings.timeout = timeout;
		}
	);
	
	this.__defineGetter__(
		"txDelay",
		function() {
			return settings.txDelay;
		}
	);
	
	this.__defineSetter__(
		"txDelay",
		function(txDelay) {
			if(typeof txDelay == "undefined" || isNaN(txDelay))
				throw "Invalid txDelay argument.";
			settings.txDelay = Math.abs(txDelay);
			sendCommand(KISS_TXDELAY, this.txDelay / 10);
		}
	);
	
	this.__defineGetter__(
		"persistence",
		function() {
			return settings.persistence / 255;
		}
	);
	
	this.__defineSetter__(
		"persistence",
		function(persistence) {
			if(
				typeof persistence == "undefined"
				||
				isNaN(persistence)
				||
				persistence > 1
			) {
				throw "Invalid persistence argument.";
			}
			settings.persistence = (persistence * 256) - 1;
			sendCommand(KISS_PERSISTENCE, settings.persistence);
		}
	);
	
	this.__defineGetter__(
		"slotTime",
		function() {
			return settings.slotTime;
		}
	);
	
	this.__defineSetter__(
		"slotTime",
		function(slotTime) {
			if(typeof slotTime == "undefined" || isNaN(slotTime))
				throw "Invalid slotTime argument.";
			settings.slotTime = Math.abs(slotTime);
			sendCommand(KISS_SLOTTIME, this.slotTime / 10);
		}
	);
	
	this.__defineGetter__(
		"txTail",
		function() {
			return settings.txTail;
		}
	);
	
	this.__defineSetter__(
		"txTail",
		function(txTail) {
			if(typeof txTail == "undefined" || isNaN(txTail))
				throw "Invalid txTail argument.";
			settings.txTail = Math.abs(txTail);
			sendCommand(KISS_TXTAIL, this.txTail / 10);
		}
	);

	this.__defineGetter__(
		"fullDuplex",
		function() {
			return (settings.fullDuplex > 0) ? true : false;
		}
	);
	
	this.__defineSetter__(
		"fullDuplex",
		function(fullDuplex) {
			if(typeof fullDuplex != "boolean")
				throw "Invalid fullDuplex argument.";
			settings.fullDuplex = fullDuplex;
			sendCommand(KISS_FULLDUPLEX, (fullDuplex) ? 1 : 0);
		}
	);
	
	this.__defineGetter__(
		"dataWaiting",
		function() {
			return (buffers.receive.length > 0) ? true : false;
		}
	);
	
	this.__defineGetter__(
		"dataPending",
		function() {
			return (buffers.send.length > 0) ? true : false;
		}
	);
	
	this.setHardware = function(setting) {
		if(typeof setting == "undefined" || isNaN(setting) || setting > 255)
			throw "Invalid hardware setting.";
		sendCommand(KISS_SETHARDWARE, setting);
	}
	
	this.exitKISS = function() {
		com.writeBin(KISS_FEND<<8|KISS_RETURN, 2);
		com.writeBin(KISS_FEND, 1);
	}

	var getPacket = function() {
		var escaped = false;
		var kissByte = com.readBin(2);
		if(kissByte != KISS_FEND<<8)
			return false;
		var kissFrame = new Array();
		var startTime = time();
		while(kissByte != KISS_FEND) {
			if(time() - startTime > settings.timeout) {
				throw format(
					"Timeout reading packet from KISS interface %s, %s-%s",
					settings.name,
					settings.callsign,
					settings.ssid
				);
			}
			kissByte = com.readBin(1);
			if(kissByte == -1)
				continue;
			if((kissByte&KISS_FESC) == KISS_FESC) {
				escaped = true;
				continue;
			}
			if(escaped && kissByte&KISS_TFESC == KISS_TFESC)
				kissFrame.push(KISS_FESC);
			else if(escaped && kissByte&KISS_TFEND == KISS_TFEND)
				kissFrame.push(KISS_FEND);
			else if(kissByte != KISS_FEND)
				kissFrame.push(kissByte);
			escaped = false;
		}
		
		var packet = new AX25.Packet(kissFrame);
		buffers.receive.push(packet);

		if(AX25.logging)
			packet.log();
			
		return true;
	}
	
	var sendPacket = function() {
		if(buffers.send.length == 0)
			return false;
		var packet = buffers.send.shift();
		var kissFrame = packet.assemble();
		var kissByte;
		com.writeBin((KISS_FEND<<8)|(settings.port<<4), 2);
		for(var i = 0; i < kissFrame.length; i++) {
			kissByte = kissFrame[i];
			if(kissByte == KISS_FEND)
				com.writeBin(KISS_FESC<<8|KISS_TFEND, 2);
			else if(kissByte == KISS_FESC)
				com.writeBin(KISS_FESC<<8|KISS_TFESC, 2);
			else
				com.writeBin(kissByte, 1);
		}
		com.writeBin(KISS_FEND, 1);
		if(AX25.logging)
			packet.log();
		return true;
	}
	
	this.receive = function() {
		return (this.dataWaiting) ? buffers.receive.shift() : undefined;
	}
	
	this.send = function(packet) {
		if(typeof packet == "undefined" || !(packet instanceof AX25.Packet))
			throw "AX25.KISSTNC: Unable to send invalid packets";
		buffers.send.push(packet);
	}
	
	this.cycle = function() {
		while(getPacket()) {
		}
		while(sendPacket()) {
		}
	}
	
	this.serialPort = serialPort;
	this.baudRate = baudRate;
	this.callsign = callsign;
	this.ssid = ssid;
	
	AX25.tncs[this.id] = this;
}

AX25.Packet = function(frame) {

	var properties = {
		'destinationCallsign'	: "",
		'destinationSSID'		: 0,
		'sourceCallsign'		: "",
		'sourceSSID'			: 0,
		'repeaterPath'			: [ ],
		'pollFinal'				: false,
		'command'				: 0,
		'response'				: 0,
		'type'					: 0,
		'nr'					: 0,
		'ns'					: 0,
		'pid'					: PID_NONE,
		'info'					: [ ]
	};
	
	this.__defineGetter__(
		"destinationCallsign",
		function() {
			if(!testCallsign(properties.destinationCallsign))
				throw "AX25.Packet: Invalid destination callsign in received packet.";
			return properties.destinationCallsign;
		}
	);
	
	this.__defineSetter__(
		"destinationCallsign",
		function(callsign) {
			if(typeof callsign == "undefined" || !testCallsign(callsign))
				throw "AX25.Packet: Invalid destination callsign assignment.";
			properties.destinationCallsign = callsign;
		}
	);
	
	this.__defineGetter__(
		"destinationSSID",
		function() {
			if(properties.destinationSSID < 0 || properties.destinationSSID > 15)
				throw "AX25.Packet: Invalid SSID in received packet.";
			return properties.destinationSSID;
		}
	);
	
	this.__defineSetter__(
		"destinationSSID",
		function(ssid) {
			if(typeof ssid == "undefined" || isNaN(ssid) || ssid < 0 || ssid > 15)
				throw "AX25.Packet: Invalid destination SSID assignment.";
			properties.destinationSSID = ssid;
		}
	);

	this.__defineGetter__(
		"sourceCallsign",
		function() {
			if(!testCallsign(properties.sourceCallsign))
				throw "AX25.Packet: Invalid source callsign in received packet.";
			return properties.sourceCallsign;
		}
	);
	
	this.__defineSetter__(
		"sourceCallsign",
		function(callsign) {
			if(typeof callsign == "undefined" || !testCallsign(callsign))
				throw "AX25.Packet: Invalid source callsign assignment.";
			properties.sourceCallsign = callsign;
		}
	);
	
	this.__defineGetter__(
		"sourceSSID",
		function() {
			if(properties.sourceSSID < 0 || properties.sourceSSID > 15)
				throw "AX25.Packet: Invalid SSID in received packet.";
			return properties.sourceSSID;
		}
	);
	
	this.__defineSetter__(
		"sourceSSID",
		function(ssid) {
			if(typeof ssid == "undefined" || isNaN(ssid) || ssid < 0 || ssid > 15)
				throw "AX25.Packet: Invalid source SSID assignment.";
			properties.sourceSSID = ssid;
		}
	);
	
	this.__defineGetter__(
		"repeaterPath",
		function() {
			return properties.repeaterPath;
		}
	);
	
	this.__defineSetter__(
		"repeaterPath",
		function(repeaters) {
			if(typeof repeaters == "undefined" || !Array.isArray(repeaters))
				throw "AX25.Packet: Repeater path must be an array of { callsign, ssid } objects.";
			for(var r = 0; r < repeaters.length; r++) {
				if(
					!repeaters[r].hasOwnProperty('callsign')
					||
					!testCallsign(repeaters[r].callsign)
				) {
					throw "AX25.Packet: Repeater path must be an array of valid { callsign, ssid } objects.";
				}
				if(
					!repeaters[r].hasOwnProperty('ssid')
					||
					repeaters[r].ssid < 0
					||
					repeaters[r].ssid > 15
				) {
					throw "AX25.Packet: Repeater path must be an array of valid { callsign, ssid } objects.";
				}
			}
			properties.repeaterPath = repeaters;
		}
	);

	this.__defineGetter__(
		"pollFinal",
		function() {
			return (properties.pollFinal == 1) ? true : false;
		}
	);
	
	this.__defineSetter__(
		"pollFinal",
		function(pollFinal) {
			if(typeof pollFinal != "boolean")
				throw "AX25.Packet: Invalid poll/final bit assignment (should be boolean.)";
			properties.pollFinal = (pollFinal) ? 1 : 0;
		}
	);

	this.__defineGetter__(
		"command",
		function() {
			return (properties.command == 1) ? true : false;
		}
	);
	
	this.__defineSetter__(
		"command",
		function(command) {
			if(typeof command != "boolean")
				throw "AX25.Packet: Invalid command bit assignment (should be boolean.)";
			properties.command = (command) ? 1 : 0;
			properties.response = (command) ? 0 : 1;
		}
	);
	
	this.__defineGetter__(
		"response",
		function() {
			return (properties.response == 1) ? true : false;
		}
	);
	
	this.__defineSetter__(
		"response",
		function(response) {
			if(typeof response != "boolean")
				throw "AX25.Packet: Invalid response bit assignment (should be boolean.)";
			properties.response = (response) ? 1 : 0;
			properties.command = (response) ? 0 : 1;
		}
	);
	
	/*	Assemble and return a control octet based on the properties of this
		packet.  (Note that there is no corresponding setter - the control
		field is always generated based on packet type, poll/final, and the
		N(S) and N(R) values if applicable, and must always be fetched from
		this getter. */
	this.__defineGetter__(
		"control",
		function() {
			var control = properties.type;
			if(properties.type == I_FRAME || (properties.type&U_FRAME) == S_FRAME)
				control|=(properties.nr<<5);
			if(properties.type == I_FRAME)
				control|=(properties.ns<<1);
			if(this.pollFinal)
				control|=(properties.pollFinal<<4);
			return control;
		}
	);

	this.__defineGetter__(
		"type",
		function() {
			return properties.type;
		}
	);
	
	this.__defineSetter__(
		"type",
		function(type) {
			if(typeof type == undefined || isNaN(type))
				throw "AX25.Packet: Invalid frame type assignment.";
			properties.type = type;
		}
	);
	
	this.__defineGetter__(
		"nr",
		function() {
			return properties.nr;
		}
	);
	
	this.__defineSetter__(
		"nr",
		function(nr) {
			if(typeof nr == "undefined" || isNaN(nr) || nr < 0 || nr > 7)
				throw "AX25.Packet: Invalid N(R) assignment.";
			properties.nr = nr;
		}
	);
	
	this.__defineGetter__(
		"ns",
		function() {
			return properties.ns;
		}
	);
	
	this.__defineSetter__(
		"ns",
		function(ns) {
			if(typeof ns == "undefined" || isNaN(ns) || ns < 0 || ns > 7)
				throw "AX25.Packet: Invalid N(S) assignment.";
			properties.ns = ns;
		}
	);
	
	this.__defineGetter__(
		"pid",
		function() {
			if(properties.pid == 0)
				return undefined;
			else
				return properties.pid;
		}
	);
	
	this.__defineSetter__(
		"pid",
		function(pid) {
			if(typeof pid == undefined || isNaN(pid))
				throw "AX25.Packet: Invalid PID field assignment.";
			if(properties.type == I_FRAME || properties.type == U_FRAME_UI)
				properties.pid = pid;
			else
				throw "AX25.Packet: PID can only be set on I and UI frames (set the frame type first, accordingly.)";
		}
	);
	
	this.__defineGetter__(
		"info",
		function() {
			if(properties.info.length < 1)
				return undefined;
			else
				return properties.info;
		}
	);
	
	this.__defineSetter__(
		"info",
		function(info) {
			if(typeof info == "undefined")
				throw "AX25.Packet: Invalid information field assignment.";
			if(properties.type == I_FRAME || properties.type == U_FRAME)
				properties.info = info;
			else
				throw "AX25.Packet: Info field can only be set on I and UI frames (set the frame type first, accordingly.)";
		}
	);
	
	/*	You can compare this value against the 'ID' properties of your
		AX25.Client objects to route an inbound packet to the appropriate
		client (or create a new one.)
		
		Example:
		
		if(AX25.clients.hasOwnProperty(packet.clientID))
			AX25.clients[packet.clientID].receivePacket(packet);
		else
			var a = new AX25.client(tnc, packet); 						*/
	this.__defineGetter__(
		'clientID',
		function() {
			return md5_calc(
				format(
					"%s%s%s%s",
					properties.sourceCallsign,
					properties.sourceSSID,
					properties.destinationCallsign,
					properties.destinationSSID
				),
				true
			);
		}
	);
	
	this.disassemble = function(frame) {
	
		// Address field
		
		// Address - destination subfield
		var field = frame.splice(0, 6);
		for(var f = 0; f < field.length; f++)
			properties.destinationCallsign += ascii(field[f]>>1);
		field = frame.shift();
		properties.destinationSSID = (field&A_SSID)>>1;
		properties.command = (field&A_CRH)>>7;
		
		// Address - source subfield
		field = frame.splice(0, 6);
		for(var f = 0; f < field.length; f++)
			properties.sourceCallsign += ascii(field[f]>>1);
		field = frame.shift();
		properties.sourceSSID = (field&A_SSID)>>1;
		properties.response = (field&A_CRH)>>7;
		
		// Address - repeater path
		while(field&1 == 0) {
			field = frame.splice(0, 6);
			var repeater = {
				'callsign' : "",
				'ssid' : 0
			};
			for(var f = 0; f < field.length; f++)
				repeater.callsign += ascii(field[f]>>1);
			field = frame.shift();
			repeater.ssid = (field&A_SSID)>>1;
			properties.repeaterPath.push(repeater);
		}
		
		// Control field
		var control = frame.shift();
		properties.pollFinal = (control&PF)>>4;
		if((control&U_FRAME) == U_FRAME) {
			properties.type = control&U_FRAME_MASK;
			if(properties.type == U_FRAME_UI) {
				properties.pid = frame.shift();
				properties.info = frame;
			}
		} else if((control&U_FRAME) == S_FRAME) {
			properties.type = control&S_FRAME_MASK;
			properties.nr = (control&NR)>>5;
		} else if((control&1) == I_FRAME) {
			properties.type = I_FRAME;
			properties.nr = (control&NR)>>5;
			properties.ns = (control&NS)>>1;
			properties.pid = frame.shift();
			properties.info = frame;
		} else {
			// This shouldn't be possible
			throw "Invalid packet.";
		}
		
	};
	
	this.assemble = function() {
	
		// Try to catch a few obvious derps
		if(properties.destinationCallsign.length == 0)
			throw "AX25.Packet: Destination callsign not set.";
		if(properties.sourceCallsign.length == 0)
			throw "AX25.Packet: Source callsign not set.";
		if(
			properties.type == I_FRAME
			&&
			(
				!properties.hasOwnProperty('pid')
				||
				!properties.hasOwnProperty('info')
				|| properties.info.length == 0
			)
		) {
			throw "I or UI frame with no payload.";
		}
		
		var frame = [];
		
		// Address field

		// Address - destination subfield - encode callsign and SSID
		for(var c = 0; c < 6; c++) {
			frame.push(
				(
					(properties.destinationCallsign.length - 1 >= c)
					?
					ascii(properties.destinationCallsign[c])
					:
					32
				)<<1
			);
		}
		frame.push((properties.command<<7)|(properties.destinationSSID<<1));

		// Address - source subfield - encode callsign and SSID
		for(var c = 0; c < 6; c++) {
			frame.push(
				(
					(properties.sourceCallsign.length - 1 >= c)
					?
					ascii(properties.sourceCallsign[c])
					:
					32
				)<<1
			);
		}
		frame.push(
			(properties.response<<7)
			|
			(properties.sourceSSID<<1)
			|
			((properties.repeaterPath.length < 1) ? 1 : 0)
		);

		// Address - tack on a repeater path if one was specified
		for(var r = 0; r < properties.repeaterPath.length; r++) {
			for(var c = 0; c < 6; c++) {
				frame.push(
					(
						(properties.repeaterPath[r].callsign.length - 1 >= c)
						?
						ascii(properties.repeaterPath[r].callsign[c])
						:
						32
					)<<1
				);
			}
			frame.push(
				(properties.repeaterPath[r].ssid<<1)
				|
				((r == properties.repeaterPath.length - 1) ? 1 : 0)
			);
		}

		// Control field
		frame.push(this.control);

		// PID field (I and UI frames only)
		if(
			properties.pid
			&&
			(properties.type == I_FRAME || properties.type == U_FRAME_UI)
		) {
			frame.push(properties.pid);
		}

		// Info field (I and UI frames only)
		if(
			properties.info.length > 0
			&&
			(properties.type == I_FRAME || properties.type == U_FRAME_UI)
		) {
			for(var i = 0; i < properties.info.length; i++)
				frame.push(properties.info[i]);
		}
		
		return frame;
	}
	
	this.log = function() {
		var out = format(
			"%s-%s -> %s-%s ",
			this.sourceCallsign,
			this.sourceSSID,
			this.destinationCallsign,
			this.destinationSSID
		);
		if(this.repeaterPath.length > 0) {
			out += "via ";
			for(var r = 0; r < this.repeaterPath.length; r++) {
				out += format(
					"%s-%s, ",
					this.repeaterPath[r].callsign,
					this.repeaterPath[r].ssid
				);
			}
		}
		switch(this.type) {
			case U_FRAME_UI:
				out += "UI";
				break;
			case U_FRAME_SABM:
				out += "SABM";
				break;
			case U_FRAME_DISC:
				out += "DISC";
				break;
			case U_FRAME_DM:
				out += "DM";
				break;
			case U_FRAME_UA:
				out += "UA";
				break;
			case U_FRAME_FRMR:
				out += "FRMR";
				break;
			case S_FRAME_RR:
				out += "RR";
				break;
			case S_FRAME_RNR:
				out += "RNR";
				break;
			case S_FRAME_REJ:
				out += "REJ";
				break;
			case I_FRAME:
				out += "I";
				break;
			default:
				// Unlikely
				out += "Unknown frame type";
				break;
		}
		
		out += format(
			", C: %s, R: %s, ",
			(this.command) ? 1 : 0,
			(this.response) ? 1 : 0
		);
		
		out += format("PF: %s", (this.pollFinal) ? 1 : 0);
		
		if((this.control&U_FRAME) == S_FRAME || (this.control&1) == I_FRAME)
			out += format(", NR: %s", this.nr);
		
		if((this.control&1) == I_FRAME)
			out += format(", NS: %s,", this.ns);
		
		if(
			(this.control&1) == I_FRAME
			||
			(this.control&U_FRAME_MASK) == U_FRAME_UI
		) {
			out += format(" PID: %s, Info: ", this.pid);
			for(var i = 0; i < this.info.length; i++)
				out += ascii(this.info[i]);
		}
		
		log(7, out);
	}
	
	if(typeof frame != "undefined")
		this.disassemble(frame);
	
}

AX25.Client = function(tnc, packet) {

	if(!(tnc instanceof AX25.KISSTNC))
		throw "AX25.Client: invalid 'tnc' argument";

	var settings = {
		'maximumRetries'	:	5,	// Maximum failed T1 or T3 polls (per timer)
		'maximumErrors'		:	5,	// Maximum (FRMR) errors before disconnect
		'timer3Interval'	:	60	// Period of inactivity before T3 poll
	};
	
	var properties = {
		'tnc'					: tnc,		// Reference to an AX25.KISSTNC obj.
		'callsign'				: "",		// Callsign of the remote side
		'ssid'					: 0,		// SSID of the remote side
		'sendState'				: 0,		// N(S) of next I frame we'll send
		'receiveState'			: 0,		// N(S) of next expected I frame
		'remoteReceiveState'	: 0,		// Remote side's last reported N(R)
		'timer1'				: 0,		// Waiting acknowledgement if not 0
		'timer1Retries'			: 0,		// Count of unacknowledged T1 polls
		'timer3Retries'			: 0,		// Count of failed keepalive polls
		'errors'				: 0,		// Persistent across link resets
		'repeaterPath'			: [],		// Same format as in AX25.Packet
		'timer3'				: time(),	// Updated on receipt of any packet
		'connected'				: false,	// True if link established
		'connecting'			: false,	// True if we sent SABM
		'disconnecting'			: false,	// True if we sent DISC
		'remoteBusy'			: false,	// True if remote sent RNR
		'rejecting'				: false,	// True if we sent FRMR
		'sentReject'			: false		// True if we sent REJ
	};
	
	var buffers = {
		'send'		: [],	// Data to be sent in I frames
		'receive'	: [],	// Data received in I frames
		'outgoing'	: [],	// Actual packets to be sent
		'incoming'	: [],	// Actual received packets
		'sent'		: []	// I frames sent but not yet acknowledged
	};
	
	/*	For internal use only.  Scripts can use AX25.Client.send() without
		having to consult this property; data will only actually be sent when
		AX25.Client.cycle() is called, and when flow control permits. */
	this.__defineGetter__(
		"canSend",
		function() {
			return (
				properties.connected
				&&
				buffers.send.length > 0
				&&
				!properties.remoteBusy
				&&
				distanceBetween(
					properties.sendState,
					properties.remoteReceiveState,
					8
				) < 7
			) ? true : false;
		}
	);
	
	this.__defineGetter__(
		"dataWaiting",
		function() {
			return (buffers.receive.length > 0) ? true : false;
		}
	);
	
	this.__defineGetter__(
		"connected",
		function() {
			return properties.connected;
		}
	);
	
	/*	When waiting for acknowledgement of (certain) sent packets, wait this
		many seconds before polling the remote side again.
		
		You may wonder how I arrived at this formula (particularly the '* 20'.)
		The AX.25 specification is deliberately vague about how long this should
		be.  Their suggested duration is far too short, resulting in our side
		pinging the remote station every couple of seconds, not leaving it
		enough time to prepare and send a response, and causing us to reach our
		retry limit in short order.  I find that ten times the round-trip time
		of the largest possible frame seems to work without being excessively
		long. ((Bits / baud rate) * hops to client) * 20. */
	this.__defineGetter__(
		"timer1Timeout",
		function() {
			return (
				(
					(AX25.maximumPacketSize / properties.tnc.baudRate)
					*
					(properties.repeaterPath.length + 1)
				)
				* 20
			);
		}
	);
	
	this.__defineGetter__(
		"callsign",
		function() {
			return properties.callsign;
		}
	);
	
	this.__defineSetter__(
		"callsign",
		function(callsign) {
			if(typeof callsign == "undefined" || !testCallsign(callsign))
				throw "AX25.Client: Invalid destination callsign assignment";
			properties.callsign = callsign;
		}
	);
	
	this.__defineGetter__(
		"ssid",
		function() {
			return properties.ssid;
		}
	);
	
	this.__defineSetter__(
		"ssid",
		function(ssid) {
			if(typeof ssid == "undefined" || isNaN(ssid) || ssid < 0 || ssid > 15)
				throw "AX25.Client: Invalid destination SSID assignment";
			properties.ssid = ssid;
		}
	);
	
	/*	You can compare the 'clientID' property of an *inbound* packet against
		this value to match it up with an existing AX25.Client object.
		
		Example:
		
		if(AX25.clients.hasOwnProperty(packet.clientID))
			AX25.clients[packet.clientID].receivePacket(packet);
		else
			var a = new AX25.Client(tnc, packet); */
	this.__defineGetter__(
		"id",
		function() {
			return md5_calc(
				format(
					"%s%s%s%s",
					properties.callsign,
					properties.ssid,
					properties.tnc.callsign,
					properties.tnc.ssid
				),
				true
			);
		}
	);
	
	// Resets properties that shouldn't persist after a link reset or disconnect
	var resetVariables = function () {
		properties.sendState			= 0;
		properties.receiveState			= 0;
		properties.remoteReceiveState	= 0;
		properties.timer1				= 0;
		properties.timer1Retries		= 0;
		properties.timer3Retries		= 0;
		properties.timer3				= time();
		properties.remoteBusy			= false;
		properties.sentReject			= false;
		properties.connected			= false;
		properties.connecting			= false;
		properties.disconnecting		= false;
	}
	
	// Returns a new AX25.Packet with the Address field pre-populated
	var makePacket = function() {
		var packet = new AX25.Packet();
		packet.destinationCallsign = properties.callsign;
		packet.destinationSSID = properties.ssid;
		packet.sourceCallsign = properties.tnc.callsign;
		packet.sourceSSID = properties.tnc.ssid;
		if(properties.repeaterPath.length > 0)
			packet.repeaterPath = properties.repeaterPath;
		return packet;
	}
	
	/*	Cancels or resets timer T1 as necessary, discards sent I frames that
		have been acknowledged by the remote side. */
	var receiveAcknowledgement = function(nr) {
		if(nr == properties.remoteReceiveState)
			return;
		properties.timer1 = 0;
		properties.timer1Retries = 0;
		properties.remoteReceiveState = nr;
		for(var i = 0; i < buffers.sent.length; i++) {
			if(buffers.sent[i].ns != wrapAround((nr - 1), 8))
				continue;
			buffers.sent.splice(0, i + 1);
		}
		if(buffers.sent.length > 0)
			properties.timer1 = time();
	}

	/*	Sends 'packet' to the TNC.  If 'packet is an I frame, stuffs it into
		buffers.sent so that it can be resent later if necessary, or discarded
		once it's acknowledged. */
	var sendPacket = function(packet) {
		properties.tnc.send(packet);
		if(packet.type == I_FRAME) {
			buffers.sent.push(packet);
			properties.timer1 = time();
		}
	}

	/*	Shifts the next payload off of buffers.send, creates an I frame around
		it, pushes that I frame into buffers.outgoing. 'command' and 'pollFinal'
		are boolean toggles for the C, R, and P/F bits of the I frame. */
	var sendIFrame = function(command, pollFinal) {
		if(buffers.send.length == 0)
			return false;
		var info = buffers.send.shift();
		var packet = makePacket();
		packet.command = command;
		packet.type = I_FRAME;
		packet.nr = properties.receiveState;
		packet.ns = properties.sendState;
		packet.pollFinal = pollFinal;
		packet.pid = PID_NONE;
		packet.info = info;
		properties.sendState = (properties.sendState + 1) % 8;
		buffers.outgoing.push(packet);
		return true;
	}
	
	/*	Resends I frames (called when the remote side sends a REJ frame.)
		receiveAcknowledgement() should be called on the receipt of any S or I
		frame and (on receipt of a REJ frame) prior to calling this function.
		That way, buffers.sent[0] will assuredly be the oldest of our sent I
		frames that we're still waiting for acknowledgement on.	*/
	var resendIFrames = function(nr) {
		if(buffers.sent.length < 1 || buffers.sent[0].ns != nr)
			return false;
		for(var i = 0; i < buffers.sent.length; i++) {
			buffers.sent[i].nr = properties.receiveState;
			buffers.outgoing.push(buffers.sent[i]);
		}
		return true;
	}
	
	this.connect = function(callsign, ssid) {
		resetVariables();
		this.callsign = callsign;
		this.ssid = (typeof ssid == "undefined") ? 0 : ssid;
		var packet = makePacket();
		packet.type = U_FRAME_SABM;
		packet.command = true;
		packet.pollFinal = true;
		buffers.outgoing.push(packet);
		properties.connecting = true;
		properties.timer1 = time();
	}
	
	this.disconnect = function() {
		resetVariables();
		for(var b in buffers)
			buffers[b] = [];
		var packet = makePacket();
		packet.type = U_FRAME_DISC;
		packet.command = true;
		packet.pollFinal = true;
		buffers.outgoing.push(packet);
		properties.disconnecting = true;
		properties.timer1 = time();
	}

	this.cycleTimers = function() {

		var packet = makePacket();
		packet.pollFinal = true;
		packet.command = true;
	
		if(
			properties.connected
			&&
			(
				properties.timer1Retries > settings.maximumRetries
				||
				properties.timer3Retries > settings.maximumRetries
			)
		) {
			packet.type = U_FRAME_DM;
			resetVariables();
		} else if(
			properties.timer1 > 0
			&&
			time() - properties.timer1 > this.timer1Timeout
		) {
			if(properties.connecting) {
				packet.type = U_FRAME_SABM;
			} else if(properties.disconnecting) {
				packet.type = U_FRAME_DISC;
			} else if(properties.connected) {
				packet.type = S_FRAME_RR;
				packet.nr = properties.receiveState;
			}
			properties.timer1Retries++;
			properties.timer1 = time();
		} else if(
			properties.connected
			&&
			time() - properties.timer3 > settings.timer3Interval
		) {
			packet.type = S_FRAME_RR;
			packet.nr = properties.receiveState;
			properties.timer3Retries++;
			properties.timer3 = time();
		} else {
			packet = false;
		}
		
		if(!packet || packet.type == I_FRAME)
			return;
		
		buffers.outgoing.push(packet);
	}
	
	this.send = function(info) {
		if(typeof info == "undefined")
			throw "AX25.Client: Tried to send an empty packet";
		buffers.send.push(info);
	}
	
	this.sendString = function(str) {
		if(typeof str != "string")
			throw "AX25.Client.sendString: Invalid string";
		buffers.send.push(stringToByteArray(str));
	}

	this.receive = function() {
		if(!this.dataWaiting)
			return undefined;
		return buffers.receive.shift();
	}
	
	this.receiveString = function() {
		if(!this.dataWaiting)
			return undefined;
		return byteArrayToString(buffers.receive.shift());
	}
	
	this.receivePacket = function(packet) {
		if(typeof packet == "undefined" || !(packet instanceof AX25.Packet))
			throw "AX25.Client: Tried to handle an invalid packet";

		if(properties.callsign == "") {
			properties.callsign = packet.sourceCallsign;
			properties.ssid = packet.sourceSSID;
			AX25.clients[this.id] = this;
		}

		buffers.incoming.push(packet);
		properties.timer3 = time();
		properties.timer3Retries = 0;
	}
	
	this.handlePacket = function(packet) {
		
		// Update the repeater path, unsetting the H bit as we go
		properties.repeaterPath = [];
		for(var r = packet.repeaterPath.length - 1; r >= 0; r--) {
			// Drop any packet that was meant for a repeater and not us
			if(packet.repeaterPath[r].ssid&A_CRH == 0)
				return false;
			packet.repeaterPath[r].ssid|=(0<<7);
			properties.repeaterPath.push(packet.repeaterPath[r]);
		}

		var response = makePacket();
		response.pollFinal = packet.pollFinal;
		response.response = packet.command;
		
		if(!properties.connected) {
		
			switch(packet.type) {
			
				case U_FRAME_SABM:
					resetVariables();
					properties.connected = true;
					response.type = U_FRAME_UA;
					break;
					
				case U_FRAME_UA:
					if(properties.connecting) {
						resetVariables();
						properties.connected = true;
						response = false;
						break;
					} // Else, fall through to default
					
				case U_FRAME_UI:
					buffers.receive.push(packet.info);
					if(!packet.pollFinal) {
						response = false;
						break;
					} // Else, fall through to default
			
				default:
					response.type = U_FRAME_DM;
					response.pollFinal = true;
					break;
					
			}
		
		} else {
		
			switch(packet.type) {
			
				case U_FRAME_SABM:
					resetVariables();
					properties.connected = true;
					response.type = U_FRAME_UA;
					break;

				case U_FRAME_DISC:
					resetVariables();
					response.type = U_FRAME_UA;
					break;
					
				case U_FRAME_UA:
					if(properties.connecting || properties.disconnecting) {
						resetVariables();
						properties.connected = (properties.connecting) ? true : false;
						response = false;
						break;
					} // Else, fall through to default
					
				case U_FRAME_UI:
					buffers.receive.push(packet.info);
					if(packet.pollFinal) {
						response.type = S_FRAME_RR;
						response.nr = properties.receiveState;
					} else {
						response = false;
					}
					break;
					
				case U_FRAME_DM:
					resetVariables();
					response = false;
					break;
					
				case U_FRAME_FRMR:
					properties.errors++;
					if(errors >= settings.maximumErrors) {
						response.type = U_FRAME_DISC;
						properties.disconnecting = true;
					} else {
						response.type = U_FRAME_SABM;
						properties.connecting = true;
					}
					properties.timer1 = time();
					break;
					
				case S_FRAME_RR:
					properties.remoteBusy = false;
					receiveAcknowledgement(packet.nr);
					if(packet.pollFinal && packet.command) {
						response.type = S_FRAME_RR;
						response.nr = properties.receiveState;
					} else {
						response = false;
					}
					break;
					
				case S_FRAME_RNR:
					properties.remoteBusy = true;
					receiveAcknowledgement(packet.nr);
					if(packet.pollFinal) {
						response.type = S_FRAME_RR;
						response.nr = properties.receiveState;
					} else {
						response = false;
					}
					break;
					
				case S_FRAME_REJ:
					properties.remoteBusy = false;
					receiveAcknowledgement(packet.nr);
					if(packet.pollFinal) {
						response.type = S_FRAME_RR;
						response.nr = properties.receiveState;
						buffers.outgoing.push(response);
					}
					if(!resendIFrames(packet.nr)) {
						response.type = U_FRAME_SABM;
						properties.connecting = true;
					} else {
						response = false;
					}
					break;
					
				case I_FRAME:
					receiveAcknowledgement(packet.nr);
					if(packet.ns != properties.receiveState) {
						if(packet.pollFinal || !properties.sentReject) {
							response.type = S_FRAME_REJ;
							response.command = true;
							response.nr = properties.receiveState;
							properties.sentReject = true;
						} else {
							response = false;
						}
					} else {
						properties.sentReject = false;
						properties.receiveState = (properties.receiveState + 1) % 8;
						buffers.receive.push(packet.info);
						if(this.canSend && !packet.pollFinal) {
							sendIFrame(false, false);
							response = false;
						} else {
							response.type = S_FRAME_RR;
							response.nr = properties.receiveState;
						}
					}
					break;
					
				default:
					response = false;
					break;
					
			}
			
		}
		
		if(response)
			buffers.outgoing.push(response);

	}
	
	this.cycle = function() {
	
		// Process any incoming packets and queue up responses
		while(buffers.incoming.length > 0) {
			this.handlePacket(buffers.incoming.shift());
		}
		
		this.cycleTimers();
		
		// Stuff any outgoing I frames into the buffer
		while(this.canSend) {
			if(!sendIFrame(true, false))
				break;
		}
		
		// Send any outgoing packets to the TNC
		while(buffers.outgoing.length > 0) {
			sendPacket(buffers.outgoing.shift());
		}
		
	}
	
	/*	If this object was instantiated with a 'packet' argument, we assume that
		it's a packet received from the remote side and stuff it into the
		incoming buffer.  When .cycle() is first called, that packet will be
		processed and responded to. */
	if(typeof packet != "undefined")
		this.receivePacket(packet);

}