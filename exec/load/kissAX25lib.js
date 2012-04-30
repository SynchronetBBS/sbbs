// kissAX25lib.js for Synchronet 3.15+
// echicken -at- bbs.electronicchicken.com (VE3XEC)
// Support for KISS TNCs and partial support for the AX.25 protocol.

load("ax25defs.js");

// Return a kissTNC object (see below) based on 'section' of ctrl/kiss.ini
function loadKISSInterface(section) {
	var f = new File(system.ctrl_dir + "kiss.ini");
	f.open("r");
	if(!f.exists || !f.is_open) return false;
	var kissINI = f.iniGetObject(section);
	f.close();
	var tnc = new kissTNC(section, kissINI.callsign, kissINI.ssid, kissINI.serialPort, Number(kissINI.baudRate));
	return tnc;
}

// Load and configure all KISS TNCs, return an array of kissTNC objects (see below)
function loadKISSInterfaces() {
	var f = new File(system.ctrl_dir + "kiss.ini");
	f.open("r");
	if(!f.exists || !f.is_open) return false;
	var kissINI = f.iniGetAllObjects();
	f.close();
	var kissTNCs = new Array();
	for(var i = 0; i < kissINI.length; i++) kissTNCs.push(new kissTNC(kissINI[i].name, kissINI[i].callsign, kissINI[i].ssid, kissINI[i].serialPort, Number(kissINI[i].baudRate)));
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
	if(!this.handle.is_open) return false;

	// Read a KISS frame from a TNC, return an AX.25 packet (array of bytes) less the flags and FCS
	this.getKISSFrame = function() {
		var escaped = false;
		var kissByte = this.handle.readBin(2);
		if(kissByte != (KISS_FEND<<8)) return false;
		var kissFrame = new Array();
		// To do: add a timeout to this loop
		while(kissByte != KISS_FEND) {
			kissByte = this.handle.readBin(1);
			if(kissByte == -1) continue;
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
		for(var i = 0; i < dest.length; i++) this.raw.push((dest[i]<<1));
		this.raw.push((parseInt(this.destinationSSID)<<1));
		var src = stringToByteArray(this.source);
		for(var i = 0; i < src.length; i++) this.raw.push((src[i]<<1));
		if(!repeaters) {
			this.raw.push((parseInt(this.sourceSSID)<<1)|(1<<0));
		} else {
			this.raw.push((parseInt(this.sourceSSID)<<1));
			for(var i = 0; i < this.repeaters.length; i++) {
				var repeater = this.repeaters[i].split("-");
				var repeaterCall = stringToByteArray(repeater[0]);
				for(var j = 0; j < repeaterCall.length; j++) this.raw.push((repeaterCall[j]<<1));
				var repeaterSSID = (parseInt(repeater[1])<<1);
				if(i == this.repeaters.length - 1) repeaterSSID |= (1<<0);
				this.raw.push(repeaterSSID);
			}
		}
		this.raw.push(this.control);
		if(this.pid !== undefined) this.raw.push(this.pid);
		if(this.information !== undefined) {
			for(var i = 0; i < this.information.length; i++) {
				this.raw.push(this.information[i]);
			}
			this.ns = ((this.control & NS)>>>1);
		}
		if((this.control & I_FRAME) == I_FRAME || (this.control & S_FRAME) == S_FRAME) this.nr = ((this.control & NR)>>>5);
	}

	this.disassemble = function(p) {
		this.raw = p;
		this.destination = "";
		for(var i = 0; i < 6; i++) this.destination += ascii((p[i]>>1));
		this.destinationSSID = ((p[6] & A_SSID)>>1);
		this.source = "";
		for(var i = 7; i < 13; i++) this.source += ascii(p[i]>>1);
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
				if(p[i] & (1<<0)) break;
			}
		}
		this.control = p[i + 1]; // Implementation can compare this against bitmasks from ax25defs.js to determine frame type.
		// A U frame or an I frame will have a PID octet and an information field
		if((this.control & U_FRAME_UI) == U_FRAME_UI || (this.control & U_FRAME_FRMR) == U_FRAME_FRMR || (this.control & I_FRAME) == I_FRAME) {
			this.pid = p[i + 2];
			this.information = new Array();
			for(var x = i + 3; x < p.length; x++) this.information.push(p[x]);
		}
		// An I frame will have N(S) (send-sequence) bits in the control octet
		if((this.control & I_FRAME) == I_FRAME) this.ns = ((this.control & NS)>>>1);
		// An I frame or an S frame will have N(R) (receive sequence) bits in the control octet
		if((this.control & I_FRAME) == I_FRAME || (this.control & S_FRAME) == S_FRAME) this.nr = ((this.control & NR)>>>5);
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
	for(var i = 0; i < s.length; i++) r.push(ascii(s[i]));
	return r;
}

// Turns an array of ASCII character codes into a string
function byteArrayToString(s) {
	var r = "";
	for(var i = 0; i < s.length; i++) r += ascii(s[i]);
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
	this.ssv = 0; // Send Sequence Variable
	this.rsv = 0; // Receive Sequence Variable
	this.ns = 0; // Client's last reported N(S)
	this.nr = 0; // Client's last reported N(R)
	this.t1 = 0; // Timer T1
	this.t2 = 0; // Timer T2
	this.t3 = 0; // Timer T3
	this.resend = false;
	this.reject = false;
	this.connected = false;
	this.sentIFrames = [];
	this.lastPacket = false;

	/*	Process and respond to ax25packet object 'p', returning false unless
		'p' is an I frame, in which case the I frame payload will be returned
		as an array of bytes.
		
		Argument 'p' is optional; if it is not supplied, this function will
		try to read an AX.25 frame from the KISS interface associated with
		this client. */
	this.receive = function(p) {
		if(p == undefined) {
			var kissFrame = this.kissTNC.getKISSFrame();
			if(!kissFrame) return false;
			var p = new ax25packet();
			p.disassemble(kissFrame);
			if(p.destination != this.kissTNC.callsign || p.destinationSSID != this.kissTNC.ssid) return false;
		}
		p.logPacket();
		var retval = false;
		var a = new ax25packet;
		if((p.control & U_FRAME_SABM) == U_FRAME_SABM) {
			this.connected = true;
			if(this.reject) {
				this.reject = false;
			} else {
				this.rsv = 0;
				this.ssv = 0;
			}
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_UA);
			log(LOG_INFO, this.kissTNC.callsign + "-" + this.kissTNC.ssid + ": Connection from " + this.callsign + "-" + this.ssid);
		} else if((p.control & U_FRAME_DM) == U_FRAME_DM) {
			this.connected = false;
		} else if((p.control & U_FRAME_UA) == U_FRAME_UA) {
			if((this.lastPacket.control & U_FRAME_SABM) == U_FRAME_SABM) this.connected = true;
			if((this.lastPacket.control & U_FRAME_DISC) == U_FRAME_DISC) this.connected = false;
			return retval;
		} else if((p.control & U_FRAME_FRMR) == U_FRAME_FRMR && this.connected) {
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_SABM);
			this.rsv = 0;
			this.ssv = 0;
		} else if((p.control & U_FRAME_DISC) == U_FRAME_DISC) {
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_UA);
			this.connected = false;
			this.reject = false;
		} else if((p.control & U_FRAME) == U_FRAME) {
			// Unnumbered Information fields should be processed, I guess.
			return retval;
		} else if(!this.connected) {
			a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_DM);
		} else if((p.control & S_FRAME_REJ) == S_FRAME_REJ) {
			this.resend = true;
			a.raw = this.sentIFrames[p.nr - 1];
			this.nr = p.nr;
		} else if((p.control & S_FRAME_RNR) == S_FRAME_RNR) {
			this.nr = p.nr;
		} else if((p.control & S_FRAME) == S_FRAME) {
			// This is a Receive-Ready and an acknowledgement of all frames in the sequence up to client's N(R)
			this.nr = p.nr;
			if(p.nr == 7 && this.sentIFrames.length == 7) {
				this.sentIFrames = []; // Client acknowledges the entire sequence, we can ditch our stored sent packets
				return retval;
			} else if(p.nr == 7 && this.sentIFrames.length > 7) {
				this.sentIFrames = this.sentIFrames.slice(7); // If we sent more I frames before they acknowledged our N(S)=7, we don't want to delete them yet.
				return retval;
			} else if(this.resend && p.nr < this.sentIFrames.length) {
				a.raw = this.sentIFrames[p.nr - 1];
			} else if(this.resend && p.nr >= this.sentIFrames.length) {
				this.resend = false;
				return retval;
			} else {
				return retval;
			}
		} else if((p.control & I_FRAME) == I_FRAME) {
			this.ns = p.ns;
			this.nr = p.nr;
			if(p.ns != this.rsv) {
				if(this.reject) return retval;
				// Send a REJ, requesting retransmission of the frame whose N(S) value matches our current RSV
				a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, (S_FRAME_REJ|(this.rsv<<5)));
				this.reject = true;
			} else if(p.information.length <= 256) {
				// This is an actual, good and expected I frame
				this.rsv++;
				if(this.rsv > 7) this.rsv = 0;
				a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, (S_FRAME_RR|(this.rsv<<5)));
				if(p.hasOwnProperty("information") || p.information.length > 0) retval = p.information;
				this.reject = false;
			} else {
				// Send a FRMR with the offending control field, our RSV and SSV, and the "Z" flag to indicate an invalid N(R)
				var i = [p.control, (this.rsv<<5)|(this.ssv<<1), 0];
				if(p.information.length > 256) i[2] = (1<<2);
				if(p.nr != this.ssv) i[2]|=(1<<3);
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
		if(this.ssv > 7) this.ssv = 0;
		this.sentIFrames.push(a);
	}

	// Connect this client to an AX.25 host, 5 attempts at 3 second intervals
	// (Attempts and intervals should probably be made configurable)
	this.connect = function() {
		var a = new ax25packet();
		a.assemble(this.callsign, this.ssid, this.kissTNC.callsign, this.kissTNC.ssid, false, U_FRAME_SABM);
		var i = 0;
		while(!c.connected && i < 5){
			this.sendPacket(a);
			mswait(3000);
			this.receive();
			i++;
		}
		if(c.connected) {
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
		while(c.connected && i < 5){
			this.sendPacket(a);
			mswait(3000);
			this.receive();
			i++;
		}
		if(c.connected) {
			c.connected = false;
			log(LOG_INFO, this.callsign + "-" + this.ssid + " failed to acknowledge U_FRAME_DISC from " + this.kissTNC.callsign + "-" + this.kissTNC.ssid);
		} else {
			log(LOG_INFO, this.callsign + "-" + this.ssid + " disconnected from " + this.kissTNC.callsign + "-" + this.kissTNC.ssid);
		}
	}
}
