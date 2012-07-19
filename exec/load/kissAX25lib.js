/*	kissAX25lib.js for Synchronet 3.15+
	echicken -at- bbs.electronicchicken.com (VE3XEC)
	
	This library provides support for the KISS and AX.25 protocols.  AX.25
	support is partial and needs much improvement.
	
	The following functions are available:
	
	loadKISSInterface(section)
		- 'section' is the name of a section from ctrl/kiss.ini
		- Returns a kissTNC object
		
	loadKISSInterfaces()
		- Takes no arguments and loads all interfaces defined in ctrl/kiss.ini
		- Returns an array of kissTNC objects
		
	logbyte(b)
		- Adds a representation of an eight-bit byte to the log (eg. 00111100)
		- Occasionally useful for debugging
		
	stringToByteArray(s)
		- Converts string 's' into an array of ASCII values
		- Returns an array with one element per character in 's'
		
	byteArrayToString(s)
		- Converts byte array 's' (as above) into a string and returns it
		
	The following objects are available:
	
	kissTNC(name, callsign, ssid, serialPort, baudRate)

		Normally you'll use loadKISSInterface(section) to instantiate these,
		however the arguments it accepts are obvious if you want to create
		instances manually and bypass kiss.ini.
		
		Properties:
		
		'name'
			A textual name for the TNC (a section heading from kiss.ini)
		
		'callsign'
			The callsign assigned to this TNC
		
		'ssid'
			The SSID assigned to this TNC
			
		'handle'
			A COM object representing the serial connection to the TNC.  This
			has its own methods for reading and writing data to and from the
			TNC.
		
		Methods:
		
		getKISSFrame()
			A bit of a misnomer.  What it actually does is read a KISS frame
			from the TNC and then returns an array representing the bytes of
			the AX.25 packet contained therein.
			
		sendKISSFrame(p)
			Where 'p' is array containing the bytes of an AX.25 frame, this
			will encapsulate that packet in a KISS frame and then send it to
			the TNC for transmission.
			
		beacon()
			Causes the TNC to transmit a UI frame containing the value of
			system.name (the name of your BBS.)  I'll probably make the beacon
			text configurable in kiss.ini later on.
			
	ax25packet()
	
		Represents a single AX.25 frame, but is mostly devoid of properties
		until either the assemble() or disassemble() methods are called.
		
		assemble(destination, destinationSSID, source, sourceSSID, repeaters, control, pid, information)
			- 'repeaters' is the digipeater path, an array of 'callsign-ssid'
			- 'control' is a control field octet from ax25defs.js, eg. U_FRAME
			- 'pid' is a protocol ID octet from ax25defs.js (usually PID_NONE)
			- 'information' is the payload of an I or UI frame, eg. the return
			  value of stringToByteArray(s)
		
		disassemble(p)
			'p' is an array containing the bytes of an AX.25 frame, eg. the
			return value of kissTNC.getKISSFrame().
		
		logPacket()
			Writes some information about this packet to the log.
			
		After assemble() or disassemble() has been called, your ax25packet
		object will have the following properties:
		
			'destination'
				The remote callsign
			
			'destinationSSID'
				The remote SSID
				
			'source'
				The local callsign
			
			'sourceSSID'
				The local SSID
			
			'repeaters'
				An array of 'callsign-ssid' representing the digipeater path
			
			'control'
				The control octet, which can be compared bitwise against masks
				defined in ax25defs.js
				
			'pid'
				The protocol ID octet, which can be compared bitwise against
				masks defined in ax25defs.js
				
			'information'
				The payload if applicable (I or UI frames only,) good for use
				with byteArrayToString(s)

			'clientID'
				A unique ID for the sender or recipient, for internal use
				
			'raw'
				An array representing the bytes of the frame, good for use
				with kissTNC.sendKISSFrame(p)
				
	ax25Client(destination, destinationSSID, source, sourceSSID, k)
	
		Can represent a client that has called you, but can also be used to
		represent an outgoing session that you have initiated.
		
		All arguments but 'k' are self-explanatory. 'k' is a reference to a
		kissTNC object (see above) and will be the interface through which all
		traffic for this client shall pass.
		
		Properties:
		
		'kissTNC'
			A reference to the kissTNC object representing the TNC through
			which all traffic to and from this client passes
		
		'callsign'
			The local callsign
		
		'ssid'
			The local SSID
			
		'clientID'
			A unique ID for this client, for internal use, same as
			ax25packet.clientID
			
		'ssv'
			The send-sequence variable
			
		'rsv'
			The receive-sequence variable
		
		'ns'
			Client's last reported N(S)
			
		'nr'
			Client's last reported N(R)
		
		't1'
			Timer T1 (Placeholder; not yet implemented)

		't2'
			Timer T2 (Placeholder; not yet implemented)

		't3'
			Timer T3 (Placeholder; not yet implemented)

		'resend'
			true/false, for internal use
			
		'reject'
			true/false, for internal use
			
		'wait'
			If true, do not send any additional I frames to this client. (Your
			scripts should check this value before deciding to send data to
			the client.)
			
		'connected'
			true/false, whether or not this client is connected

		'sentIFrames'
			An array of the last seven I frames that we have sent to this
			client.

		'lastPacket'
			An ax25packet object representing the last packet that we sent to
			this client.
			
		Methods:
		
		receive(p)
			'p' is an optional ax25packet object; if omitted, an attempt will
			be made to read an AX.25 frame from the KISS TNC associated with
			this client.  Returns false if there is no AX.25 frame waiting to
			be received.
		
		sendPacket(a)
			'a' is an ax25packet object that will be sent to the client via
			its associated KISS TNC.
		
		send(p)
			Sends an I frame to the client, where 'p' is the payload (eg. a
			return value from stringToByteArray(s).
		
		connect()
			Makes five attempts to connect to the client, at three second
			intervals.  (Attempts and intervals to be made configurable at
			some point.)  To verify the connection state afterward, check
			the value of ax25client.connected.
		
		disconnect()
			Makes five attempts to disconnect the client, at three second
			intervals.  (Attempts and intervals to be made configurable at
			some point.  To verify the connection state afterward, check the
			value of ax25client.connected.
		
*/

load("ax25defs.js");

// Return a kissTNC object (see below) based on 'section' of ctrl/kiss.ini
function loadKISSInterface(section) {
	var f = new File(system.ctrl_dir + "kiss.ini");
	f.open("r");
	if(!f.exists || !f.is_open)
		return false;
	var kissINI = f.iniGetObject(section);
	f.close();
	var tnc = new kissTNC(section, kissINI.callsign, kissINI.ssid, kissINI.serialPort, Number(kissINI.baudRate));
	return tnc;
}

// Load and configure all KISS TNCs, return an array of kissTNC objects (see below)
function loadKISSInterfaces() {
	var f = new File(system.ctrl_dir + "kiss.ini");
	f.open("r");
	if(!f.exists || !f.is_open)
		return false;
	var kissINI = f.iniGetAllObjects();
	f.close();
	var kissTNCs = new Array();
	for(var i = 0; i < kissINI.length; i++) {
		kissTNCs.push(new kissTNC(kissINI[i].name, kissINI[i].callsign, kissINI[i].ssid, kissINI[i].serialPort, Number(kissINI[i].baudRate)));
	}
	return kissTNCs;
}

// Create an object representing a KISS TNC, where object.handle is a COM object
function kissTNC(name, callsign, ssid, serialPort, baudRate) {
	this.name = name;
	this.callsign = callsign;
	this.ssid = ssid;
	this.handle = new COM(serialPort);
	this.handle.baud_rate = parseInt(baudRate);
	this.handle.open();
	if(!this.handle.is_open)
		return false;

	// Read a KISS frame from a TNC, return an AX.25 packet (array of bytes) less the flags and FCS
	this.getKISSFrame = function() {
		var escaped = false;
		var kissByte = this.handle.readBin(2);
		if(kissByte != (KISS_FEND<<8))
			return false;
		var kissFrame = new Array();
		// To do: add a timeout to this loop
		while(kissByte != KISS_FEND) {
			kissByte = this.handle.readBin(1);
			if(kissByte == -1)
				continue;
			if((kissByte & KISS_FESC) == KISS_FESC) {
				escaped = true;
				continue;
			}
			if(escaped && (kissByte & KISS_TFESC) == KISS_TFESC) {
				kissFrame.push(KISS_FESC);
			} else if(escaped && (kissByte & KISS_TFEND) == KISS_TFEND) {
				kissFrame.push(KISS_FEND);
			} else if(kissByte != KISS_FEND) {
				kissFrame.push(kissByte);
			}
			escaped = false;
		}
		return kissFrame;
	}

	// Write a KISS frame to a TNC where p is an AX.25 packet (array of bytes) less the flags and FCS
	this.sendKISSFrame = function(p) {
		if(p == undefined) return false;
		var kissByte;
		this.handle.writeBin((KISS_FEND<<8), 2);
		for(var i = 0; i < p.length; i++) {
			kissByte = p[i];
			if(kissByte == KISS_FEND) {
				this.handle.writeBin((KISS_FESC<<8)|KISS_TFEND, 2);
			} else if(kissByte == KISS_FESC) {
				this.handle.writeBin((KISS_FESC<<8)|KISS_TFESC, 2);
			} else {
				this.handle.writeBin(kissByte, 1);
			}
		}
		this.handle.writeBin(KISS_FEND, 1);
	}
	
	this.beacon = function() {
		var a = new ax25packet();
		a.assemble("BEACON", 0, this.callsign, this.ssid, false, U_FRAME_UI, PID_NONE, stringToByteArray(system.name));
		this.sendKISSFrame(a.raw);
	}
}
	
function logByte(b) {
	log(format("%d%d%d%d%d%d%d%d\r\n", (b & (1<<7)) ? 1 : 0, (b & (1<<6)) ? 1 : 0, (b & (1<<5)) ? 1 : 0, (b & (1<<4)) ? 1 : 0, (b & (1<<3)) ? 1 : 0, (b & (1<<2)) ? 1 : 0, (b & (1<<1)) ? 1 : 0, (b & (1<<0)) ? 1 : 0 ));
}

function ax25packet() {

	this.assemble = function(destination, destinationSSID, source, sourceSSID, repeaters, control, pid, information) {
		this.destination = destination;
		this.destinationSSID = destinationSSID;
		this.source = source;
		this.sourceSSID = sourceSSID;
		this.repeaters = repeaters;
		this.control = control;
		this.pid = pid;
		this.information = information;
		this.clientID = this.destination.replace(/\s/, "") + this.destinationSSID + this.source.replace(/\s/, "") + this.sourceSSID;
		this.raw = new Array();
		var dest = stringToByteArray(this.destination);
		for(var i = 0; i < dest.length; i++) {
			this.raw.push((dest[i]<<1));
		}
		this.raw.push((parseInt(this.destinationSSID)<<1));
		var src = stringToByteArray(this.source);
		for(var i = 0; i < src.length; i++) {
			this.raw.push((src[i]<<1));
		}
		if(!repeaters) {
			this.raw.push((parseInt(this.sourceSSID)<<1)|(1<<0));
		} else {
			this.raw.push((parseInt(this.sourceSSID)<<1));
			for(var i = 0; i < this.repeaters.length; i++) {
				var repeater = this.repeaters[i].split("-");
				var repeaterCall = stringToByteArray(repeater[0]);
				for(var j = 0; j < repeaterCall.length; j++) {
					this.raw.push((repeaterCall[j]<<1));
				}
				var repeaterSSID = (parseInt(repeater[1])<<1);
				if(i == this.repeaters.length - 1)
					repeaterSSID |= (1<<0);
				this.raw.push(repeaterSSID);
			}
		}
		this.raw.push(this.control);
		if(this.pid !== undefined)
			this.raw.push(this.pid);
		if(this.information !== undefined) {
			for(var i = 0; i < this.information.length; i++) {
				this.raw.push(this.information[i]);
			}
			this.ns = ((this.control & NS)>>>1);
		}
		if((this.control & I_FRAME) == I_FRAME || (this.control & S_FRAME) == S_FRAME)
			this.nr = ((this.control & NR)>>>5);
	}

	this.disassemble = function(p) {
		this.raw = p;
		this.destination = "";
		for(var i = 0; i < 6; i++) {
			this.destination += ascii((p[i]>>1));
		}
		this.destinationSSID = ((p[6] & A_SSID)>>1);
		this.source = "";
		for(var i = 7; i < 13; i++) {
			this.source += ascii(p[i]>>1);
		}
		var i = 13;
		this.sourceSSID = ((p[i] & A_SSID)>>1);
		this.clientID = this.destination.replace(/\s/, "") + this.destinationSSID + this.source.replace(/\s/, "") + this.sourceSSID;
		// Either the source callsign & SSID pair was the end of the address field, or we need to tack on a repeater path
		if(p[i] & (1<<0)) {
			this.repeaters = [0];
		} else {
			var repeater = "";
			this.repeaters = new Array();
			for(var i = 14; i <= 78; i++) {
				if(repeater.length == 6) {
					repeater += "-" + ((p[i] & A_SSID)>>1);
					this.repeaters.push(repeater);
					repeater = "";
				} else {
					repeater += ascii((p[i]>>1));
				}
				if(p[i] & (1<<0))
					break;
			}
		}
		this.control = p[i + 1]; // Implementation can compare this against bitmasks from ax25defs.js to determine frame type.
		// A U frame or an I frame will have a PID octet and an information field
		if((this.control & U_FRAME_UI) == U_FRAME_UI || (this.control & U_FRAME_FRMR) == U_FRAME_FRMR || (this.control & I_FRAME) == I_FRAME) {
			this.pid = p[i + 2];
			this.information = new Array();
			for(var x = i + 3; x < p.length; x++) {
				this.information.push(p[x]);
			}
		}
		// An I frame will have N(S) (send-sequence) bits in the control octet
		if((this.control & I_FRAME) == I_FRAME)
			this.ns = ((this.control & NS)>>>1);
		// An I frame or an S frame will have N(R) (receive sequence) bits in the control octet
		if((this.control & I_FRAME) == I_FRAME || (this.control & S_FRAME) == S_FRAME)
			this.nr = ((this.control & NR)>>>5);
	}
	
	this.logPacket = function() {
		var x = "Unknown or unhandled frame type";
		if((this.control & U_FRAME_SABM) == U_FRAME_SABM) {
			x = "U_FRAME_SABM";
		} else if((this.control & U_FRAME_UA) == U_FRAME_UA) {
			x = "U_FRAME_UA";
		} else if((this.control & U_FRAME_FRMR) == U_FRAME_FRMR) {
			x = "U_FRAME_FRMR";
		} else if((this.control & U_FRAME_DISC) == U_FRAME_DISC) {
			x = "U_FRAME_DISC";
		} else if((this.control & U_FRAME) == U_FRAME) {
			x = "U_FRAME";
		} else if((this.control & S_FRAME_REJ) == S_FRAME_REJ) {
			x = "S_FRAME_REJ, N(R): " + this.nr;
		} else if((this.control & S_FRAME_RNR) == S_FRAME_RNR) {
			x = "S_FRAME_RNR, N(R): " + this.nr;
		} else if((this.control & S_FRAME) == S_FRAME) {
			x = "S_FRAME, N(R): " + this.nr;
		} else if((this.control & I_FRAME) == I_FRAME) {
			x = "I_FRAME, N(R): " + this.nr + ", N(S): " + this.ns;
		}
		log(LOG_DEBUG, this.source + "-" + this.sourceSSID + "->" + this.destination + "-" + this.destinationSSID + ": " + x);
	}
}

// Turns a string into an array of ASCII character codes
function stringToByteArray(s) {
	s = s.split("");
	var r = new Array();
	for(var i = 0; i < s.length; i++) {
		r.push(ascii(s[i]));
	}
	return r;
}

// Turns an array of ASCII character codes into a string
function byteArrayToString(s) {
	var r = "";
	for(var i = 0; i < s.length; i++) {
		r += ascii(s[i]);
	}
	return r;
}

// Create an AX.25 client object from an ax25packet() object
function ax25Client(destination, destinationSSID, source, sourceSSID, k) {
	this.kissTNC = k;
	if(source == k.callsign && sourceSSID == k.ssid) {
		this.callsign = destination;
		this.ssid = destinationSSID;
	} else {
		this.callsign = source;
		this.ssid = sourceSSID;	
	}
	this.clientID = destination.replace(/\s/, "") + destinationSSID + source.replace(/\s/, "") + sourceSSID;

	this.init = function() {
		this.ssv = 0; // Send Sequence Variable
		this.rsv = 0; // Receive Sequence Variable
		this.ns = 0; // Client's last reported N(S)
		this.nr = 0; // Client's last reported N(R)
		this.t1 = 0; // Timer T1
		this.t2 = 0; // Timer T2
		this.t3 = 0; // Timer T3
		this.resend = false;
		this.reject = false;
		this.wait = false;
		this.connected = false;
		this.sentIFrames = [];
		this.lastPacket = false;
		this.expectUA = false;
	}
	
	this.init();

	/*	Process and respond to ax25packet object 'p', returning false unless
		'p' is an I frame, in which case the I frame payload will be returned
		as an array of bytes.
		
		Argument 'p' is optional; if it is not supplied, this function will
		try to read an AX.25 frame from the KISS interface associated with
		this client. */
	this.receive = function(p) {
		if(p == undefined) {
			var kissFrame = this.kissTNC.getKISSFrame();
			if(!kissFrame)
				return false;
			var p = new ax25packet();
			p.disassemble(kissFrame);
			if(p.destination != this.kissTNC.callsign || p.destinationSSID != this.kissTNC.ssid)
				return false;
		}
		p.logPacket();
		var retval = false;
		var a = new ax25packet;
		if((p.control & U_FRAME_SABM) == U_FRAME_SABM) {
			this.connected = true;
			if(this.reject) {
				this.reject = false;
			} else {
				this.init();
				this.connected = true;
			}
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_UA);
			log(LOG_INFO, this.kissTNC.callsign + "-" + this.kissTNC.ssid + ": Connection from " + this.callsign + "-" + this.ssid);
		} else if((p.control & U_FRAME_DM) == U_FRAME_DM) {
			this.connected = false;
		} else if((p.control & U_FRAME_UA) == U_FRAME_UA) {
			if(this.expectUA) {
				this.expectUA = false;
				if((this.lastPacket.control & U_FRAME_SABM) == U_FRAME_SABM)
					this.connected = true;
				if((this.lastPacket.control & U_FRAME_DISC) == U_FRAME_DISC)
					this.connected = false;
				return retval;
			} else {
				a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_SABM);
				this.init();
				this.expectUA = true;
			}
		} else if((p.control & U_FRAME_FRMR) == U_FRAME_FRMR && this.connected) {
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_SABM);
			this.init();
		} else if((p.control & U_FRAME_DISC) == U_FRAME_DISC) {
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_UA);
			this.connected = false;
			this.reject = false;
		} else if((p.control & U_FRAME) == U_FRAME) {
			if(p.hasOwnProperty("information") && p.information.length > 0)
				retval = p.information;
			return retval;
		} else if(!this.connected) {
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_DM);
		} else if((p.control & S_FRAME_REJ) == S_FRAME_REJ) {
			this.resend = true;
			a = this.sentIFrames[p.nr - 1];
			this.nr = p.nr;
		} else if((p.control & S_FRAME_RNR) == S_FRAME_RNR) {
			this.nr = p.nr - 1;
		} else if((p.control & S_FRAME) == S_FRAME) {
			// This is a Receive-Ready and an acknowledgement of all frames in the sequence up to client's N(R)
			this.nr = p.nr;
			if(this.ssv >= this.nr)
				// We haven't exceeded the flow control window, so no need to wait before sending more I frames
				this.wait = false;
			if(p.nr == 7 && this.sentIFrames.length >= 7) {
				// Client acknowledges the entire sequence, we can ditch our stored sent packets
				this.sentIFrames = this.sentIFrames.slice(7);
				return retval;
			} else if(this.resend && p.nr < this.sentIFrames.length) {
				a = this.sentIFrames[p.nr - 1];
			} else if(this.resend && p.nr >= this.sentIFrames.length) {
				this.resend = false;
				return retval;
			} else {
				return retval;
			}
		} else if((p.control & I_FRAME) == I_FRAME) {
			this.ns = p.ns;
			this.nr = p.nr;
			if(this.ssv >= this.nr)
				this.wait = false;
			if(p.ns != this.rsv) {
				if(this.reject)
					return retval;
				// Send a REJ, requesting retransmission of the frame whose N(S) value matches our current RSV
				a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, (S_FRAME_REJ|(this.rsv<<5)));
				this.reject = true;
			} else if(p.information.length <= 256) {
				// This is an actual, good and expected I frame
				this.rsv++;
				this.rsv = this.rsv % 8;
				a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, (S_FRAME_RR|(this.rsv<<5)));
				if(p.hasOwnProperty("information") && p.information.length > 0)
					retval = p.information;
				this.reject = false;
			} else {
				// Send a FRMR with the offending control field, our RSV and SSV, and the "Z" flag to indicate an invalid N(R)
				var i = [p.control, (this.rsv<<5)|(this.ssv<<1), 0];
				if(p.information.length > 256)
					i[2] = (1<<2);
				if(p.nr != this.ssv)
					i[2]|=(1<<3);
				a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, (U_FRAME_FRMR), PID_NONE, i);
				this.reject = true;
			}
		} else {
			return retval;
		}
		this.sendPacket(a);
		return retval;
	}

	// Send ax25Packet object 'a' to an ax25Client
	this.sendPacket = function(a) {
		this.lastPacket = a;
		this.kissTNC.sendKISSFrame(a.raw);
		a.logPacket();
	}

	// Send an I Frame to an ax25Client, with payload 'p'
	this.send = function(p) {
		var a = new ax25packet();
		a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, (I_FRAME|(this.rsv<<5)|(this.ssv<<1)), PID_NONE, p);
		this.sendPacket(a);
		this.ssv++;
		this.ssv = this.ssv % 8;
		if(this.ssv < this.nr)
			/*	If we send again, we will exceed the flow control window. We
				should wait for the client to catch up before sending more. */
			this.wait = true;
		this.sentIFrames.push(a);
	}

	// Connect this client to an AX.25 host, 5 attempts at 3 second intervals
	// (Attempts and intervals should probably be made configurable)
	this.connect = function() {
		var a = new ax25packet();
		a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_SABM);
		var i = 0;
		while(!this.connected && i < 5){
			this.sendPacket(a);
			this.expectUA = true;
			mswait(3000);
			this.receive();
			i++;
		}
		if(this.connected) {
			log(LOG_INFO, this.kissTNC.callsign + "-" + this.kissTNC.ssid + " connected to " + this.callsign + "-" + this.ssid);
		} else {
			log(LOG_INFO, this.kissTNC.callsign + "-" + this.kissTNC.ssid + " failed to connect to " + this.callsign + "-" + this.ssid);
		}
	}

	// Disconnect this client, 5 attempts at 3 second intervals
	// (Attempts and intervals should probably be made configurable)
	this.disconnect = function() {
		var a = new ax25packet();
		a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_DISC);
		var i = 0;
		while(this.connected && i < 5){
			this.sendPacket(a);
			this.expectUA = true;
			mswait(3000);
			this.receive();
			i++;
		}
		if(this.connected) {
			this.connected = false;
			log(LOG_INFO, this.callsign + "-" + this.ssid + " failed to acknowledge U_FRAME_DISC from " + this.kissTNC.callsign + "-" + this.kissTNC.ssid);
		} else {
			log(LOG_INFO, this.callsign + "-" + this.ssid + " disconnected from " + this.kissTNC.callsign + "-" + this.kissTNC.ssid);
		}
	}
}
