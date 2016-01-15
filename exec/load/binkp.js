load("sockdefs.js");
load("fido.js");

/*
 * A binkp implementation...
 * 
 * Create a new instance with New passing a path to place received files
 * in to the constructor (defaults to system.temp_dir).
 * 
 * Next, adjust defaults as needed...
 * default_zone    - if no zone is specified, use this one for all addresses.
 * default_domain  - if no domain is specified, use this one for all addresses.
 * debug		   - If set, logs all sent/received frames via log(LOG_DEBUG)
 * require_md5	   - Require that the remote support MD5
 * timeout		   - Max timeout
 * addr_list       - list of addresses handled by this system.  Defaults to system.fido_addr_list
 * system_name	   - BBS name to send to remote defaults to system.name
 * system_operator - SysOp name to send to remote defaults to system.operator
 * system_location - System location to send to remote defaults to system.location
 * want_callback   - Callback when a file is offered... can rename file, skip file,
 *                   or ACK file without receiving it.
 *                   Parameters: File object, file size, file date, offset
 *                   Return values: this.file.SKIP   - Skips the file, will be retransmitted later.
 *									this.file.ACCEPT - open()s the file and receives it.
 *									this.file.REJECT - Refuses to take the file... will not be retransmitted later.
 *                   Default value is this.default_want() which accepts all offered
 *					 files.
 * rx_callback	   - Function that is called with a single filename argument
 *                   when a file is received successfully.
 *                   Intended for REQ/TIC processing.  This callback can call
 * 				     the addFile(filename) method (may not work unless
 *					 ver1_1 is true)
 * name_ver        - Name and version of program in "name/ver.ver.ver" format
 * 
 * Now add any files you wish to send using the addFile(filename) method
 * 
 * Finally, call the connect() or accept() method
 * This method will return true if all files were transferred with no errors.
 * 
 * After return, the sent_files and received_files arrays will contain
 * lists of successfully transferred files.  The failed_sent_files and
 * failed_received_files arrays will contain files that failed to
 * transfer.
 */

function BinkP(name_ver, inbound, rx_callback)
{
	var addr;

	if (name_ver === undefined)
		name_ver = 'UnknownScript/0.0';
	this.name_ver = name_ver;

	if (inbound === undefined)
		inbound = system.temp_dir;
	this.inbound = backslash(inbound);

	this.rx_callback = rx_callback;

	this.default_zone = 1;
	addr = FIDO.parse_addr(system.fido_addr_list[0], this.default_zone);
	this.default_zone = addr.zone;
	this.senteob = false;
	this.goteob = false;
	this.pending_ack = [];
	this.pending_get = [];
	this.tx_queue=[];
	this.debug = false;
	this.nonreliable = false;
	this.sent_nr = false;
	this.ver1_1 = false;
	this.require_md5 = true;
	this.timeout = 120;
	this.addr_list = [];
	this.system_name = system.name;
	this.system_operator = system.operator;
	this.system_location = system.location;
	system.fido_addr_list.forEach(function(faddr){this.addr_list.push(FIDO.parse_addr(faddr, this.default_zone));}, this);
	this.want_callback = this.default_want;

	this.sent_files = [];
	this.failed_sent_files = [];

	this.received_files = [];
	this.failed_received_files = [];
}
BinkP.prototype.Frame = function() {};
BinkP.prototype.default_want = function(fobj, fsize, fdate, offset)
{
	// Reject duplicate filenames... a more robust callback would rename them.
	// Or process the old ones first.
	if (this.received_files.indexOf(fobj.name) != -1)
		return this.file.REJECT;
	// Skip existing files.
	if (file_exists(fobj.name))
		return this.file.SKIP;
	// Accept everything else
	return this.file.ACCEPT;
};
BinkP.prototype.escapeFileName = function(name)
{
	return name.replace(/[^A-Za-z0-9!"#$%&'\(\)*+,\-.\/:;<=>?@\[\]\^_`{|}~]/g, function(match) { return format('\\x%02x', ascii(match)); });
};
BinkP.prototype.unescapeFileName = function(name)
{
	return name.replace(/\\x?([0-9a-fA-F]{2})/g, function(match, val) { return ascii(parseInt(val, 16)); });
};
BinkP.prototype.command = {
	M_NUL:0,
	M_ADR:1,
	M_PWD:2,
	M_FILE:3,
	M_OK:4,
	M_EOB:5,
	M_GOT:6,
	M_ERR:7,
	M_BSY:8,
	M_GET:9,
	M_SKIP:10,
};
BinkP.prototype.command_name = [
	"M_NUL",
	"M_ADR",
	"M_PWD",
	"M_FILE",
	"M_OK",
	"M_EOB",
	"M_GOT",
	"M_ERR",
	"M_BSY",
	"M_GET",
	"M_SKIP"
];
BinkP.prototype.ack_file = function()
{
	if (this.receiving !== undefined) {
		if (this.receiving.position >= this.receiving_len) {
			this.receiving.truncate(this.receiving_len);
			this.receiving.close();
			this.receiving.date = this.receiving_date;
			if (this.rx_callback !== undefined)
				this.rx_callback(this.receiving.name, this);
			if (this.sendCmd(this.command.M_GOT, this.escapeFileName(this.receiving_name)+' '+this.receiving.length+' '+this.receiving.date))
				this.received_files.push(this.receiving.name);
		}
		else {
			this.receiving.close();
			this.failed_received_files.push(this.receiving.name);
		}
		this.receiving = undefined;
		this.receiving_len = undefined;
		this.receiving_date = undefined;
		this.receiving_name = undefined;
	}
};
BinkP.prototype.getCRAM = function(algo, key)
{
	var tmp;

	if (algo !== 'MD5')
		return undefined;

	function binary_md5(key)
	{
		return md5_calc(key, true).replace(/[0-9a-fA-F]{2}/g,function(m){return ascii(parseInt(m, 16));});
	}

	function str_xor(str1, val)
	{
		var i;
		var ret='';

		for (i=0; i<str1.length; i++) {
			ret += ascii(str1.charCodeAt(i) ^ val);
		}
		return ret;
	}

	tmp = key;
	if (tmp.length > 64)
		tmp = binary_md5(tmp);
	while(tmp.length < 64)
		tmp += '\x00';
	tmp = md5_calc(str_xor(tmp, 0x5c) + binary_md5(str_xor(tmp, 0x36) + this.cram.challenge), true);
	return 'CRAM-'+algo+'-'+tmp;
};
BinkP.prototype.parseArgs = function(data)
{
	var ret = data.split(/ /);
	var i;

	for (i=0; i<ret.length; i++)
		ret[i] = this.unescapeFileName(ret[i]);
	return ret;
};
/*
 * auth_cb(response, this) is called to add files the response parameter is the
 * parameter string send with the M_OK message... hopefully either "secure"
 * or "non-secure"
 */
BinkP.prototype.connect = function(addr, password, auth_cb, port)
{
	var pkt;

	if (addr === undefined)
		throw("No address specified!");
	addr = FIDO.parse_addr(addr, this.default_zone, this.default_domain);

	if (password === undefined)
		password = '-';

	if (port === undefined)
		port = 24554;

	if (this.sock === undefined)
		this.sock = new Socket(SOCK_STREAM, "binkp");

	if(!this.sock.connect(addr.inet_host, port)) {
		this.sock = undefined;
		return false;
	}

	this.authenticated = undefined;
	this.sendCmd(this.command.M_NUL, "SYS "+this.system_name);
	this.sendCmd(this.command.M_NUL, "ZYZ "+this.system_operator);
	this.sendCmd(this.command.M_NUL, "LOC "+this.system_location);
	this.sendCmd(this.command.M_NUL, "NDL 115200,TCP,BINKP");
	this.sendCmd(this.command.M_NUL, "TIME "+new Date().toString());
	this.sendCmd(this.command.M_NUL, "VER "+this.name_ver+",JSBinkP/"+("$Revision$".split(' ')[1])+'/'+system.platform+" binkp/1.1");
	this.sendCmd(this.command.M_ADR, this.addr_list.join(' '));

	while(!js.terminated && this.remote_addrs === undefined) {
		pkt = this.recvFrame(this.timeout);
		if (pkt === undefined)
			return false;
	}

	if (this.authenticated === undefined) {
		if (this.cram === undefined || this.cram.algo !== 'MD5') {
			if (this.require_md5)
				this.sendCmd(this.command.M_ERR, "MD5 Required");
			else
				this.sendCmd(this.command.M_PWD, password);
		}
		else {
			this.sendCmd(this.command.M_PWD, this.getCRAM(this.cram.algo, password));
		}
	}

	while((!js.terminated) && this.authenticated === undefined) {
		pkt = this.recvFrame(this.timeout);
		if (pkt === undefined)
			return false;
	}

	auth_cb(this.authenticated, this);

	if (js.terminated) {
		this.close();
		return false;
	}
	return this.session();
};
/*
 * sock can be either a lisening socket or a connected socket.
 *
 * auth_cb(addrs, passwds, challenge, this) is called to accept and add
 * files if it returns true, the session is considered secure.  auth_cb()
 * is explicitly allowed to change the inbound property and call
 * this.sendCmd(this.command.M_ERR, "Error String");
 *
 * It is up to the auth_cb() callback to enforce the require_md5 property.
 */
BinkP.prototype.accept = function(sock, auth_cb)
{
	var challenge='';
	var i;
	var pkt;

	if (sock === undefined || auth_cb === undefined)
		return false;

	if (this.sock !== undefined)
		this.close();

	if (sock.is_connected)
		this.sock = sock;
	else
		this.sock = sock.accept();

	if (this.sock == undefined || !this.sock.is_connected)
		return false;

	for (i=0; i<128; i++)
		challenge += random(16).toString(16);

	this.authenticated = undefined;
	this.sendCmd(this.command.M_NUL, "OPT CRAM-MD5-"+challenge);
	this.sendCmd(this.command.M_NUL, "SYS "+this.system_name);
	this.sendCmd(this.command.M_NUL, "ZYZ "+this.system_operator);
	this.sendCmd(this.command.M_NUL, "LOC "+this.system_location);
	this.sendCmd(this.command.M_NUL, "NDL 115200,TCP,BINKP");
	this.sendCmd(this.command.M_NUL, "TIME "+new Date().toString());
	this.sendCmd(this.command.M_NUL, "VER JSBinkP/0.0.0/"+system.platform+" binkp/1.1");
	this.sendCmd(this.command.M_ADR, this.addr_list.join(' '));

	while(!js.terminated && this.authenticated === undefined) {
		pkt = this.recvFrame(this.timeout);
		if (pkt === undefined)
			return false;
		if (pkt !== null && pkt !== this.partialFrame) {
			if (pkt.is_cmd) {
				if (pkt.command === this.command.M_PWD) {
					if (auth_cb(this.remote_addrs, this.parseArgs(pkt.data), challenge, this))
						this.authenticated = 'secure';
					else
						this.authenticated = 'non-secure';
					this.sendCmd(this.command.M_OK, this.authenticated);
					break;
				}
			}
		}
	}

	if (js.terminated) {
		this.close();
		return false;
	}
	return this.session();
};
BinkP.prototype.file = {
	SKIP:0,
	ACCEPT:1,
	REJECT:2
};
BinkP.prototype.session = function()
{
	var i;
	var pkt;
	var m;
	var tmp;
	var tmp2;
	var ver;
	var args;
	var size;
	var last = Date.now();

	// Session set up, we're good to go!
	outer:
	while(!js.terminated && this.sock !== undefined) {
		// We want to wait if we have no more files to send or if we're
		// skipping files.
		pkt = this.recvFrame(this.senteob ? this.timeout : 0);
		if (pkt !== undefined && pkt !== this.partialFrame && pkt !== null) {
			last = Date.now();
			if (pkt.is_cmd) {
				cmd_switch:
				switch(pkt.command) {
					case this.command.M_NUL:
						// Ignore
						break;
					case this.command.M_FILE:
						this.senteob = false;
						this.ack_file();
						args = this.parseArgs(pkt.data);
						if (args.length < 4) {
							log(LOG_ERROR, "Invalid M_FILE command args: '"+pkt.data+"'");
							this.sendCmd(this.command.M_ERR, "Invalid M_FILE command args: '"+pkt.data+"'");
						}
						tmp = new File(this.inbound + file_getname(args[0]));
						switch (this.want_callback(tmp, parseInt(args[1], 10), parseInt(args[2], 10), parseInt(args[3], 10), this)) {
							case this.file.SKIP:
								this.sendCmd(this.command.M_SKIP, this.escapeFileName(args[0])+' '+args[1]+' '+args[2]);
								break;
							case this.file.REJECT:
								this.sendCmd(this.command.M_GOT, this.escapeFileName(args[0])+' '+args[1]+' '+args[2]);
								break;
							case this.file.ACCEPT:
								size = file_size(tmp.name);
								if (size == -1)
									size = 0;
								if (parseInt(args[3], 10) < 0) {
									// Non-reliable mode...
									if (this.nonreliable || this.ver1_1) {
										this.sendCmd(this.command.M_GET, this.escapeFileName(args[0])+' '+args[1]+' '+args[2]+' '+size);
									}
									else {
										log(LOG_ERR, "M_FILE Offset of -1 in reliable mode!");
										this.sendCmd(this.command.M_ERR, "M_FILE Offset of -1 in reliable mode!");
									}
								}
								else {
									if (parseInt(args[3], 10) > size) {
										log(LOG_ERR, "Invalid offset of "+args[3]+" into file '"+tmp.name+"' size "+size);
										this.sendCmd(this.command.M_ERR, "Invalid offset of "+args[3]+" into file '"+tmp.name+"' size "+size);
									}
									if (!tmp.open(tmp.exists ? "r+b" : "wb")) {
										log(LOG_ERROR, "Unable to open file "+tmp.name);
										this.sendCmd(this.command.M_SKIP, this.escapeFileName(args[0])+' '+args[1]+' '+args[2]);
										break;
									}
									this.receiving = tmp;
									this.receiving_name = args[0];
									this.receiving_len = parseInt(args[1], 10);
									this.receiving_date = parseInt(args[2], 10);
								}
								break;
							default:
								log(LOG_ERR, "Invalid return value from want_callback!");
								this.sendCmd(this.command.M_ERR, "Implementation bug at my end, sorry.");
						}
						break;
					case this.command.M_EOB:
						this.ack_file();
						if (this.senteob)
							break outer;
						break;
					case this.command.M_GOT:
						args = this.parseArgs(pkt.data);
						for (i=0; i<this.pending_ack.length; i++) {
							if (this.pending_ack[i].sendas == args[0]) {
								this.sent_files.push(this.pending_ack[i].file.name);
								this.pending_ack.splice(i, 1);
								i--;
							}
						}
						break;
					case this.command.M_GET:
						args = this.parseArgs(pkt.data);
						// If we already sent this file, ignore the command...
						for (i=0; i<this.sent_files; i++) {
							if (file_getname(this.sent_files[i]) === args[0])
								break cmd_switch;
						}
						// Now look for it in failed...
						for (i=0; i<this.failed_sent_files.length; i++) {
							if (file_getname(this.failed_sent_files[i]) === args[0]) {
								// Validate the size, date, and offset...
								if (file_size(this.failed_sent_files[i]) != parseInt(args[1], 10) || file_date(this.failed_sent_files[i]) != parseInt(args[2], 10) || file_size(this.failed_sent_files[i]) < parseInt(args[3], 10))
									break;
								// Re-add it
								this.addFile(this.failed_sent_files[i]);
								// And remove from failed list
								this.failed_sent_files.splice(i, 1);
								break;
							}
						}
						// Now, simply adjust the position in a pending file
						for (i=0; i<this.tx_queue.length; i++) {
							if (file_getname(this.tx_queue[i].file.name) === args[0]) {
								// Validate the size, date, and offset...
								if (this.tx_queue[i].file.length != parseInt(args[1], 10) || this.tx_queue[i].file.date != parseInt(args[2], 10) || this.tx_queue[i].file.length < parseInt(args[3], 10))
									break;
								this.tx_queue[i].file.position = parseInt(args[3], 10);
							}
						}
						break;
					case this.command.M_SKIP:
						args = this.parseArgs(pkt.data);
						for (i=0; i<this.pending_ack.length; i++) {
							if (this.pending_ack[i].sendas == args[0]) {
								this.pending_ack.splice(i, 1);
								i--;
							}
						}
						if (file_getname(this.sending.sendas) === args[0]) {
							this.sending.file.close();
							this.failed_sent_files.push(this.sending.file.name);
							this.sending = undefined;
						}
						break;
					default:
						if (pkt.command < this.command_name.length)
							tmp = this.command_name[pkt.command];
						else
							tmp = 'Unknown Command '+pkt.command;
						log(LOG_ERROR, "Unhandled "+tmp+" command from remote. Aborting session.");
						this.sendCmd(this.command.M_ERR, "Unhandled command.");
				}
			}
			else {
				// DATA packet...
				if (this.receiving === undefined) {
					if (this.debug)
						log(LOG_DEBUG, "Data packet outside of file!");
				}
				else {
					this.receiving.write(pkt.data);
					// We need to ACK here...
					this.ack_file();
				}
			}
		}
		if (this.sending === undefined && !this.senteob) {
			this.sending = this.tx_queue.shift();
			if (this.sending === undefined) {
				this.sendCmd(this.command.M_EOB);
			}
			else {
				this.pending_ack.push(this.sending);
				if (this.nonreliable)
					this.sendCmd(this.command.M_FILE, this.escapeFileName(this.sending.sendas)+' '+this.sending.file.length+' '+this.sending.file.date+' -1');
				else
					this.sendCmd(this.command.M_FILE, this.escapeFileName(this.sending.sendas)+' '+this.sending.file.length+' '+this.sending.file.date+' '+this.sending.file.position);
				if (this.ver1_1)
					this.goteob = false;
			}
		}
		if (this.sending !== undefined) {
			if(this.sendData(this.sending.file.read(32767)))
				last = Date.now();
			if (this.eof || this.sending.file.position >= this.sending.file.length) {
				this.sending.file.close();
				this.sending = undefined;
			}
		}

		if ((last + this.timeout)*1000 < Date.now())
			this.sendCmd(this.command.M_ERR, "Timeout exceeded!");
	}

	this.close();

	if (js.terminated)
		return false;
	return true;
};
BinkP.prototype.close = function()
{
	var i;

	// Send an ERR and close.
	this.ack_file();
	// Any still pending have failed.
	for (i=0; i<this.pending_ack.length; i++)
		this.failed_sent_files.push(this.pending_ack[i].file.name);
	for (i=0; i<this.tx_queue.length; i++)
		this.failed_sent_files.push(this.tx_queue[i].file.name);
	if ((!this.goteob) || this.tx_queue.length || this.pending_ack.length || this.pending_get.length) {
		this.sendCmd(this.command.M_ERR, "Forced Shutdown");
	}
	else {
		if (!this.senteob) {
			this.sendCmd(this.command.M_EOB);
		}
	}
	this.tx_queue.forEach(function(file) {
		file.file.close();
	});
};
BinkP.prototype.sendCmd = function(cmd, data)
{
	var type;
	var tmp;

	if (this.sock === undefined)
		return false;
	if (data === undefined)
		data = '';
	if (this.debug) {
		if (cmd < this.command_name.length)
			type = this.command_name[cmd];
		else
			type = 'Unknown Command '+cmd;
		log(LOG_DEBUG, "Sent "+type+" command args: "+data);
	}
	var len = data.length+1;
	len |= 0x8000;
	if (!this.sock.sendBin(len, 2))
		return false;
	if (!this.sock.sendBin(cmd, 1))
		return false;
	if (!this.sock.send(data))
		return false;
	switch(cmd) {
		case this.command.M_EOB:
			this.senteob=true;
			break;
		case this.command.M_ERR:
		case this.command.M_BSY:
			this.sock.close();
			this.sock = undefined;
			break;
		case this.command.M_NUL:
			if (data.substr(0, 4) === 'OPT ') {
				tmp = data.substr(4).split(/ /);
				if (tmp.indexOf('NR'))
					this.sent_nr = true;
			}
			break;
	}
	return true;
};
BinkP.prototype.sendData = function(data)
{
	var len = data.length;

	if (this.sock === undefined)
		return false;
	if (this.debug)
		log(LOG_DEBUG, "Sending "+data.length+" bytes of data");
	if (!this.sock.sendBin(len, 2))
		return false;
	if (!this.sock.send(data))
		return false;
	return true;
};
BinkP.prototype.recvFrame = function(timeout)
{
	var ret;
	var type;
	var i;
	var args;
	var options;
	var tmp;
	var ver;
	var avail;

	// Avoid warning from syncjslint by putting this in a closure.
	function hex2ascii(hex)
	{
		return ascii(parseInt(hex, 16));
	}

	if (this.sock === undefined) {
		this.partialFrame = undefined;
		return undefined;
	}

	if (timeout === undefined)
		timeout = 0;

	if (this.partialFrame === undefined) {
		ret = new this.Frame();
		if (!this.sock.poll(timeout))
			return null;
		ret.length = this.sock.recvBin(2);
		ret.is_cmd = (ret.length & 0x8000) ? true : false;
		ret.length &= 0x7fff;
		ret.data = '';
	}
	else
		ret = this.partialFrame;

	if (this.sock.poll(timeout)) {
		avail = this.sock.nread;
		if (avail == 0) {
			if (this.sock.is_connected)
				log(LOG_ERROR, "Poll returned true, but no data available on connected socket!");
			this.sock.close();
			this.sock = undefined;
			return undefined;
		}
		if (avail > (ret.length - ret.data.length))
			avail = ret.length - ret.data.length;
		ret.data += this.sock.recv(avail);
	}

	if (ret.data.length < ret.length) {
		this.partialFrame = ret;
	}
	else {
		this.partialFrame = undefined;
		if (ret.is_cmd) {
			ret.command = ret.data.charCodeAt(0);
			ret.data = ret.data.substr(1);
		}
		if (this.debug) {
			if (ret.is_cmd) {
				if (ret.command < this.command_name.length)
					type = this.command_name[ret.command];
				else
					type = 'Unknown Command '+ret.command;
				log(LOG_DEBUG, "Got "+type+" command args: "+ret.data);
			}
			else
				log(LOG_DEBUG, "Got data frame length "+ret.length);
		}
		if (ret.is_cmd) {
			switch(ret.command) {
				case this.command.M_ERR:
					log(LOG_ERROR, "BinkP got fatal error from remote: '"+ret.data+"'.");
					this.sock.close();
					this.socket = undefined;
					return undefined;
				case this.command.M_BSY:
					log(LOG_WARNING, "BinkP got non-fatal error from remote: '"+ret.data+"'.");
					this.sock.close();
					this.socket = undefined;
					return undefined;
				case this.command.M_EOB:
					this.goteob = true;
					break;
				case this.command.M_ADR:
					if (this.remote_addrs !== undefined) {
						this.sendCmd(this.command.M_ERR, "Address already received.");
						return undefined;
					}
					else {
						this.remote_addrs = [];
						ret.data.split(/ /).forEach(function(addr) {
							this.remote_addrs.push(FIDO.parse_addr(addr, this.default_zone));
						}, this);
					}
					break;
				case this.command.M_OK:
					if (this.authenticated !== undefined) {
						this.sendCmd(this.command.M_ERR, "Authentication already complete.");
						return undefined;
					}
					else
						this.authenticated = ret.data;
					break;
				case this.command.M_NUL:
					args = ret.data.split(/ /);
					switch(args[0]) {
						case 'OPT':
							for (i=1; i<args.length; i++) {
								if (args[i].substr(0,9) === 'CRAM-MD5-') {
									this.cram = {algo:'MD5', challenge:args[i].substr(9).replace(/[0-9a-fA-F]{2}/g, hex2ascii)};
								}
								else {
									if (args[i] === 'NR') {
										if (!this.sent_nr)
											this.sendCmd(this.command.M_NUL, "NR");
										this.non_reliable = true;
										break;
									}
								}
							}
							break;
						case 'VER':
							tmp = ret.data.split(/ /);
							if (tmp.length >= 3) {
								if (tmp[2].substr(0, 6) === 'binkp/') {
									ver = tmp[2].substr(6).split(/\./);
									if (ver.length >= 2) {
										tmp = parseInt(ver[0], 10);
										switch(tmp) {
											case NaN:
												break;
											case 1:
												if (parseInt(ver[1], 10) > 0)
													this.ver1_1 = true;
												break;
											default:
												if (tmp > 1)
													this.ver1_1 = true;
												break;
										}
									}
								}
							}
					}
			}
		}
	}
	return ret;
};
BinkP.prototype.addFile = function(path, sendas)
{
	var file = new File(path);

	if (sendas === undefined)
		sendas = file_getname(path);
	if (!file.open("rb", true))
		return false;
	if (this.ver1_1)
		this.senteob = false;
	this.tx_queue.push({file:file, sendas:sendas});
};
