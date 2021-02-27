// $Id: binkp.js,v 1.123 2020/04/10 07:01:24 rswindell Exp $

require('sockdefs.js', 'SOCK_STREAM');
require('fido.js', 'FIDO');

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
 * require_md5	   - Require that the remote support CRAM-MD5 authentication
 * plain_auth_only - Use plain-text authentication always (no CRAM-MD5 auth, no encryption)
 * crypt_support   - Encryption supported
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
 * rx_callback	   - Function that is called with two arguments, the filename
 * 					 and the BinkP object when a file is received successfully.
 *                   Intended for REQ/TIC processing.  This callback can call
 * 				     the addFile(filename) method (may not work unless
 *					 ver1_1 is true)
 * tx_callback	   - Function that is called with two arguments, the filename
 * 					 and the BinkP object when a file is sent successfully.
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

function BinkP(name_ver, inbound, rx_callback, tx_callback)
{
	var addr;

	if (name_ver === undefined)
		name_ver = 'UnknownScript/0.0';
	this.name_ver = name_ver;
	this.revision = "JSBinkP/" + "$Revision: 1.123 $".split(' ')[1];
	this.full_ver = name_ver + "," + this.revision + ',sbbs' + system.version + system.revision + '/' + system.platform;

	if (inbound === undefined)
		inbound = system.temp_dir;
	this.inbound = backslash(inbound);

	this.rx_callback = rx_callback;
	this.tx_callback = tx_callback;

	this.default_zone = 1;
	addr = FIDO.parse_addr(system.fido_addr_list[0], this.default_zone);
	this.default_zone = addr.zone;
	this.senteob = 0;
	this.goteob = 0;
	this.pending_ack = [];
	this.pending_get = [];
	this.tx_queue=[];
	this.debug = false;
	this.nonreliable = false;
	this.sent_nr = false;
	this.ver1_1 = false;
	this.require_md5 = true;
	this.plain_auth_only = false;
	this.crypt_support = true;
	// IREX VER Internet Rex 2.29 Win32 (binkp/1.1) doesn't work with longer challenges
	// TODO: Remove this knob
	this.cram_challenge_length = 16;
	this.require_crypt = true;
	this.timeout = 120;
	this.addr_list = [];
	this.system_name = system.name;
	this.system_operator = system.operator;
	this.system_location = system.location;
	system.fido_addr_list.forEach(function(faddr){this.addr_list.push(FIDO.parse_addr(faddr, this.default_zone, 'fidonet'));}, this);
	this.want_callback = this.default_want;
	this.wont_crypt = false;
	this.will_crypt = false;
	this.in_keys = undefined;
	this.out_keys = undefined;
	this.capabilities = '115200,TCP,BINKP';
	this.remote_ver = undefined;
	this.remote_operator = undefined;
	this.remote_capabilities = undefined;
	this.remote_info = {};
	this.connect_host = undefined;
	this.connect_port = undefined;
	this.connect_error = undefined;

	this.sent_files = [];
	this.failed_sent_files = [];

	this.received_files = [];
	this.failed_received_files = [];
}
BinkP.prototype.crypt = {
	crc32tab:[	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
				0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
				0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
				0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
				0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
				0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
				0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
				0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
				0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
				0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
				0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
				0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
				0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
				0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
				0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
				0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
				0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
				0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
				0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
				0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
				0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
				0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
				0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
				0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
				0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
				0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
				0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
				0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
				0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
				0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
				0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
				0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
				0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
				0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
				0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
				0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
				0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
				0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
				0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
				0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
				0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
				0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
				0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
				0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
				0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
				0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
				0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
				0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
				0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
				0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
				0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
				0x2d02ef8d
	],
	uint:function(val) {
		val &= 0xffffffff;
		if (val < 0)
			return (val + 0x100000000);
		return val;
	},
	crc32:function(c, b) {
		var idx = (c & 0xff) ^ (b & 0xff);
		var sft = (c >> 8) & 0xffffff;
		var ret = this.uint(this.crc32tab[idx] ^ sft);
		return ret;
	},
	mult32:function(x, y) {
		var total;

		total = (x * (y & 0xff)) & 0xffffffff;
		total += (((x * ((y >> 8) & 0xff)) & 0xffffff) << 8);
		total += (((x * ((y >> 16) & 0xff)) & 0xffff) << 16);
		total += (((x * ((y >> 24) & 0xff)) & 0xff) << 24);

		return (this.uint(total));
	},
	update_keys:function(keys, c) {
		var keyshift;

		keys[0] = this.crc32(keys[0], ascii(c));
		keys[1] += (keys[0] & 0xff);
		keys[1] = this.mult32(keys[1], 134775813) + 1;
		keyshift = this.uint(keys[1] >> 24);
		keys[2] = this.crc32(keys[2], keyshift);
		return ascii(c);
	},
	init_keys:function(keys, passwd) {
		var i;

		keys[0] = 305419896;
		keys[1] = 591751049;
		keys[2] = 878082192;
		for (i=0; i<passwd.length; i++)
			this.update_keys(keys, passwd[i]);
	},
	decrypt_byte:function(keys) {
		var temp;

		temp = (keys[2] & 0xffff) | 2;
		return ((temp * (temp ^ 1)) >> 8) & 0xff;
	},
	decrypt_buf:function(buf, keys) {
		var i;
		var ret = '';
		var ch;

		for (i=0; i<buf.length; i++) {
			ch = ascii(ascii(buf[i]) ^ this.decrypt_byte(keys));
			ret += ch;
			this.update_keys(keys, ch);
		}
		return ret;
	},
	encrypt_buf:function(buf, keys) {
		var t;
		var i;
		var ret = '';

		for (i=0; i<buf.length; i++) {
			t = this.decrypt_byte(keys);
			this.update_keys(keys, buf[i]);
			ret += ascii(ascii(buf[i]) ^ t);
		}
		return ret;
	},
};
BinkP.prototype.reset_eob = function(also_sent) {
	if (this.ver1_1) {
		this.goteob = 0;
		if (also_sent)
			this.senteob = 0;
	}
};
BinkP.prototype.send_chunks = function(str) {
	var ret;
	var sent = 0;

	while (sent < str.length) {
		ret = this.sock.send(str.substr(sent));
		if (ret > 0)
			sent += ret;
		else
			return false;
	}
	return true;
};
BinkP.prototype.send_buf = function(str) {
	if (this.out_keys === undefined)
		return str;
	return this.crypt.encrypt_buf(str, this.out_keys);
};
BinkP.prototype.recv_buf = function(str) {
	if (this.in_keys === undefined)
		return str;
	return this.crypt.decrypt_buf(str, this.in_keys);
};
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
	var cb_success = true;
	var gotlen;

	if (this.receiving !== undefined) {
		if (this.receiving.position >= this.receiving_len) {
			this.receiving.truncate(this.receiving_len);
			gotlen = this.receiving.position;
			this.receiving.close();
			this.receiving.date = this.receiving_date;
			if (this.rx_callback !== undefined)
				cb_success = this.rx_callback(this.receiving.name, this);
			if (cb_success) {
				if (this.sendCmd(this.command.M_GOT, this.escapeFileName(this.receiving_name)+' '+gotlen+' '+this.receiving_date))
					this.received_files.push(this.receiving.name);
				else {
					this.failed_received_files.push(this.receiving.name);
					log(LOG_WARNING, "Could not send M_GOT for '"+this.receiving.name+"'.");
				}
			}
			else {
				if (this.sendCmd(this.command.M_SKIP, this.escapeFileName(this.receiving_name)+' '+this.receiving_len+' '+this.receiving_date)) {
					this.failed_received_files.push(this.receiving.name);
					log(LOG_WARNING, "Callback returned false for '"+this.receiving.name+"'.");
				}
				else {
					this.failed_received_files.push(this.receiving.name);
					log(LOG_WARNING, "Could not send M_SKIP for '"+this.receiving.name+"'.");
				}
			}
		}
		else {
			log(LOG_WARNING, "Failed to receive the whole file '"+this.receiving.name+"'.");
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
BinkP.prototype.connect = function(addr, password, auth_cb, port, inet_host, tls)
{
	var pkt;
	var i;

	this.outgoing = true;
	this.will_crypt = false;
	this.in_keys = undefined;
	this.out_keys = undefined;
	if (addr === undefined)
		throw("No address specified!");
	addr = FIDO.parse_addr(addr, this.default_zone, this.default_domain);

	if (!password)
		password = '-';
	if (password === '-')
		this.require_md5 = false;
	if (port === undefined)
		port = addr.binkp_port;
	if (inet_host === undefined)
		inet_host = addr.inet_host;

	log(LOG_INFO, format("Connecting to %s at %s:%u", addr, inet_host, port));
	this.connect_host = inet_host;
	this.connect_port = port;

	if (js.global.ConnectedSocket != undefined) {
		if (this.sock !== undefined)
			this.sock.close();
		try {
			this.sock = new ConnectedSocket(inet_host, port, {protocol:'binkp'});
		}
		catch(e) {
			log(LOG_WARNING, "Connection to "+inet_host+":"+port+" failed ("+e+").");
			this.connect_error = e;
			this.sock = undefined;
			return false;
		}
	}
	else {
		if (this.sock === undefined)
			this.sock = new Socket(SOCK_STREAM, "binkp");

		if(!this.sock.connect(inet_host, port)) {
			this.connect_error = this.sock.error;
			this.sock = undefined;
			log(LOG_WARNING, "Connection to "+inet_host+":"+port+" failed.");
			return false;
		}
	}

	log(LOG_DEBUG, "Connection to "+inet_host+":"+port+" successful");

	if(tls === true) {
		log(LOG_INFO, "Negotiating TLS");
		this.sock.ssl_session = true;
	}

	this.authenticated = undefined;
	if (this.crypt_support && !this.plain_auth_only && password !== '-')
		this.sendCmd(this.command.M_NUL, "OPT CRYPT");
	else {
		/*
		 * TODO: This is to work around an apparent incompatibility with
		 * Radius.  I thought this worked with binkd, but it would need
		 * to be tested again.
		 *
		 * Not super-important since using encryption without a password
		 * is about as "secure" as rot13.
		 */
		this.wont_crypt = true;
		this.require_crypt = false;
	}
	this.sendCmd(this.command.M_NUL, "SYS "+this.system_name);
	this.sendCmd(this.command.M_NUL, "ZYZ "+this.system_operator);
	this.sendCmd(this.command.M_NUL, "LOC "+this.system_location);
	this.sendCmd(this.command.M_NUL, "NDL "+this.capabilities);
	this.sendCmd(this.command.M_NUL, "TIME "+new Date().toString());
	this.sendCmd(this.command.M_NUL, "VER "+this.full_ver + " binkp/1.1");
	this.sendCmd(this.command.M_ADR, this.addr_list.join(' '));

	while(!js.terminated && this.remote_addrs === undefined) {
		pkt = this.recvFrame(this.timeout);
		if (pkt === undefined || pkt === null)
			return false;
	}

	if (this.authenticated === undefined) {
		if (this.plain_auth_only) {
			this.sendCmd(this.command.M_PWD, password);
		}
		else if (this.cram === undefined || this.cram.algo !== 'MD5') {
			if (this.require_md5)
				this.sendCmd(this.command.M_ERR, "CRAM-MD5 authentication required");
			else {
				if (this.will_crypt)
					this.sendCmd(this.command.M_ERR, "Encryption requires CRAM-MD5 auth");
				else
					this.sendCmd(this.command.M_PWD, password);
			}
		}
		else {
			this.sendCmd(this.command.M_PWD, this.getCRAM(this.cram.algo, password));
		}
	}

	while((!js.terminated) && this.authenticated === undefined) {
		pkt = this.recvFrame(this.timeout);
		if (pkt === undefined || pkt === null)
			return false;
	}

	if (password !== '-')
		this.authenticated = 'secure';
	else
		this.authenticated = 'non-secure';

	if (auth_cb !== undefined)
		auth_cb(this.authenticated, this);

	if (this.will_crypt) {
		if (this.cram === undefined || this.cram.algo !== 'MD5')
			this.sendCmd(this.command.M_ERR, "Encryption requires CRAM-MD5 auth");
		else {
			log(LOG_DEBUG, "Initializing crypt keys.");
			this.out_keys = [0, 0, 0];
			this.in_keys = [0, 0, 0];
    		this.crypt.init_keys(this.out_keys, password);
			this.crypt.init_keys(this.in_keys,  "-");
			for (i=0; i<password.length; i++)
				this.crypt.update_keys(this.in_keys, password[i]);
		}
	}
	else {
		if (this.require_crypt && !this.wont_crypt)
			this.sendCmd(this.command.M_ERR, "Encryption required");
	}

	if (js.terminated) {
		this.close();
		return false;
	}
	return this.session();
};
/*
 * sock can be either a listening socket or a connected socket.
 *
 * auth_cb(passwds, this) is called to accept and add
 * files if it returns a password, the session is considered secure.  auth_cb()
 * is explicitly allowed to change the inbound property and call
 * this.sendCmd(this.command.M_ERR, "Error String");
 *
 * It may also set/clear the require_crypt property.
 *
 * It is up to the auth_cb() callback to enforce the require_md5 property.
 */
BinkP.prototype.accept = function(sock, auth_cb)
{
	var challenge='';
	var i;
	var pkt;
	var pwd;
	var args;

	this.outgoing = false;
	this.will_crypt = false;
	this.in_keys = undefined;
	this.out_keys = undefined;
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

	// IREX VER Internet Rex 2.29 Win32 (binkp/1.1) doesn't work with challenges longer than 32 chars
	for (i=0; i < this.cram_challenge_length * 2; i++)
		challenge += random(16).toString(16);

	// Avoid warning from syncjslint by putting this in a closure.
	function hex2ascii(hex)
	{
		return ascii(parseInt(hex, 16));
	}

	this.cram = {algo:'MD5', challenge:challenge.replace(/[0-9a-fA-F]{2}/g, hex2ascii)};
	this.authenticated = undefined;
	if(!this.crypt_support || this.plain_auth_only)
		this.wont_crypt = true;
	if(!this.plain_auth_only)
		this.sendCmd(this.command.M_NUL, "OPT CRAM-MD5-"+challenge+(this.wont_crypt?"":" CRYPT"));
	pkt = this.recvFrame(this.timeout);
	if (pkt === undefined || pkt === null)
		return false;
	this.sendCmd(this.command.M_NUL, "SYS "+this.system_name);
	this.sendCmd(this.command.M_NUL, "ZYZ "+this.system_operator);
	this.sendCmd(this.command.M_NUL, "LOC "+this.system_location);
	this.sendCmd(this.command.M_NUL, "NDL 115200,TCP,BINKP");
	this.sendCmd(this.command.M_NUL, "TIME "+new Date().toString());
	this.sendCmd(this.command.M_NUL, "VER "+this.full_ver + " binkp/1.1");
	this.sendCmd(this.command.M_ADR, this.addr_list.join(' '));

	while(!js.terminated && this.authenticated === undefined) {
		pkt = this.recvFrame(this.timeout);
		if (pkt === undefined || pkt === null)
			return false;
		if (pkt !== null && pkt !== this.partialFrame) {
			if (pkt.is_cmd) {
				if (pkt.command === this.command.M_PWD) {
					args = this.parseArgs(pkt.data);
					if (this.will_crypt) {
						if (args[0].substr(0, 9) !== 'CRAM-MD5-')
							this.sendCmd(this.command.M_ERR, "Encryption requires CRAM-MD5 auth");
					}
					pwd = auth_cb(args, this);
					if (pwd === undefined)
						pwd = '-';
					if (pwd === '-') {
						this.authenticated = 'non-secure';
						/*
						 * It appears the binkd won't encrypt unless there's
						 * a password, even if they requested CRYPT mode.
						 */
						this.will_crypt = false;
					}
					else
						this.authenticated = 'secure';
					this.sendCmd(this.command.M_OK, this.authenticated);
					break;
				}
			}
		}
	}

	if (this.will_crypt) {
		log(LOG_DEBUG, "Initializing crypt keys.");
		this.out_keys = [0, 0, 0];
		this.in_keys = [0, 0, 0];
		this.crypt.init_keys(this.in_keys, pwd);
		this.crypt.init_keys(this.out_keys,  "-");
		for (i=0; i<pwd.length; i++)
			this.crypt.update_keys(this.out_keys, pwd[i]);
	}
	else {
		if (this.require_crypt)
			this.sendCmd(this.command.M_ERR, "Encryption required");
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
	var cur_timeout;

	// Session set up, we're good to go!
	outer:
	while(!js.terminated && this.sock !== undefined) {
		// We want to wait if we have no more files to send or if we're
		// skipping files.
		cur_timeout = 0;
		if (this.ver1_1) {
			// Don't increase the timeout until we've sent the second M_EOB
			if (this.senteob >= 2)
				cur_timeout = this.timeout;
		}
		else {
			if (this.senteob)
				cur_timeout = this.timeout;
		}
		if (this.sending !== undefined && this.sending.waitingForGet !== undefined && this.sending.waitingForGet)
			cur_timeout = this.timeout;
		pkt = this.recvFrame(cur_timeout);
		if (pkt !== undefined && pkt !== this.partialFrame && pkt !== null) {
			last = Date.now();
			if (pkt.is_cmd) {
				cmd_switch:
				switch(pkt.command) {
					case this.command.M_NUL:
						// Ignore
						break;
					case this.command.M_FILE:
						this.ack_file();
						args = this.parseArgs(pkt.data);
						if (args.length < 4) {
							log(LOG_ERROR, "Invalid M_FILE command args: '"+pkt.data+"' from: " + this.remote_addrs);
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
										log(LOG_WARNING, "M_FILE Offset of -1 in reliable mode (bug in remote)!");
										this.sendCmd(this.command.M_GET, this.escapeFileName(args[0])+' '+args[1]+' '+args[2]+' '+size);
									}
								}
								else {
									if (parseInt(args[3], 10) > size) {
										log(LOG_ERR, "Invalid offset of "+args[3]+" into file '"+tmp.name+"' size "+size + " from remote: " + this.remote_addrs);
										this.sendCmd(this.command.M_ERR, "Invalid offset of "+args[3]+" into file '"+tmp.name+"' size "+size);
									}
									if (!tmp.open(tmp.exists ? "r+b" : "wb")) {
										log(LOG_ERROR, "Error " + tmp.error + " opening file "+tmp.name  + " from remote: " + this.remote_addrs );
										this.sendCmd(this.command.M_SKIP, this.escapeFileName(args[0])+' '+args[1]+' '+args[2]);
										break;
									}
									this.receiving = tmp;
									this.receiving_name = args[0];
									this.receiving_len = parseInt(args[1], 10);
									this.receiving_date = parseInt(args[2], 10);
									log(LOG_INFO, "Receiving file: " + this.receiving.name + format(" (%1.1fKB)", this.receiving_len / 1024.0));
								}
								break;
							default:
								log(LOG_ERR, "Invalid return value from want_callback from remote: " + this.remote_addrs);
								this.sendCmd(this.command.M_ERR, "Implementation bug at my end, sorry.");
						}
						break;
					case this.command.M_EOB:
						this.ack_file();
						if (this.pending_ack.length > 0)
							log(LOG_WARNING, "We got an M_EOB, but there are still "+this.pending_ack.length+" files pending M_GOT");
						else {
							if (this.ver1_1) {
								if (this.senteob >= 2 && this.goteob >= 2)
									break outer;
							}
							else {
								if (this.senteob > 0 && this.goteob > 0)
									break outer;
							}
						}
						break;
					case this.command.M_GOT:
						args = this.parseArgs(pkt.data);
						for (i=0; i<this.pending_ack.length; i++) {
							if (this.pending_ack[i].sendas === args[0]) {
								this.sent_files.push(this.pending_ack[i].file.name);
								if (this.tx_callback !== undefined)
									this.tx_callback(this.pending_ack[i].file.name, this);
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
						// Is this the current "sending" file?
						if (this.sending !== undefined && this.sending.sendas === args[0]) {
							// Stop waiting for a M_GET
							this.sending.waitingForGet = false;
							// Now, simply adjust the position in the sending file
							this.sending.file.position = parseInt(args[3], 10);
							// And send the new M_FILE
							this.sendCmd(this.command.M_FILE, this.escapeFileName(this.sending.sendas)+' '+this.sending.file.length+' '+this.sending.file.date+' '+this.sending.file.position);
							break;
						}
						// Now look for it in failed...
						for (i=0; i<this.failed_sent_files.length; i++) {
							if (file_getname(this.failed_sent_files[i].sendas) === args[0]) {
								// Validate the size, date, and offset...
								if (file_size(this.failed_sent_files[i].path) != parseInt(args[1], 10) || file_date(this.failed_sent_files[i].path) != parseInt(args[2], 10) || file_size(this.failed_sent_files[i].path) < parseInt(args[3], 10))
									break;
								// Re-add it
								this.addFile(this.failed_sent_files[i].path, this.failed_sent_files[i].sendas, false);
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
								this.failed_sent_files.push({path:this.pending_ack[i].file.name, sendas:this.pending_ack[i].sendas});
								this.pending_ack.splice(i, 1);
								i--;
							}
						}
						if (this.sending !== undefined && this.sending.sendas === args[0]) {
							this.sending.file.close();
							this.sending = undefined;
						}
						break;
					default:
						if (pkt.command < this.command_name.length)
							tmp = this.command_name[pkt.command];
						else
							tmp = 'Unknown Command '+pkt.command;
						log(LOG_ERROR, "Unhandled "+tmp+" command from remote: " + this.remote_addrs);
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
					if (this.receiving.position >= this.receiving_len)
						this.ack_file();
				}
			}
		}
		if (this.sending === undefined) {
			this.sending = this.tx_queue.shift();
			if (this.sending === undefined) {
				if (this.receiving === undefined) {
					if (this.ver1_1) {
						if (this.senteob == 0 || (this.goteob))
							this.sendCmd(this.command.M_EOB);
					}
					else {
						if (!this.senteob)
							this.sendCmd(this.command.M_EOB);
					}
				}
			}
			else {
				this.pending_ack.push(this.sending);
				log(LOG_INFO, "Sending file: " + this.sending.file.name + format(" (%1.1fKB)", this.sending.file.length / 1024.0));
				if (this.nonreliable && (this.sending.waitingForGet === undefined || this.sending.waitingForGet)) {
					this.sendCmd(this.command.M_FILE, this.escapeFileName(this.sending.sendas)+' '+this.sending.file.length+' '+this.sending.file.date+' -1');
					this.sending.waitingForGet = true;
				}
				else {
					this.sendCmd(this.command.M_FILE, this.escapeFileName(this.sending.sendas)+' '+this.sending.file.length+' '+this.sending.file.date+' '+this.sending.file.position);
					this.sending.waitingForGet = false;
				}
			}
		}
		if (this.sending !== undefined) {
			if (this.ver1_1 || this.senteob === 0) {
				if (this.sending.waitingForGet !== undefined && !this.sending.waitingForGet) {
					if(this.sendData(this.sending.file.read(32767)))
						last = Date.now();
					if (this.eof || this.sending.file.position >= this.sending.file.length) {
						log(LOG_INFO, "Sent file: " + this.sending.file.name + format(" (%1.1fKB)", this.sending.file.position / 1024.0));
						this.sending.file.close();
						this.sending = undefined;
					}
				}
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
		this.failed_sent_files.push({path:this.pending_ack[i].file.name, sendas:this.pending_ack[i].sendas});
	for (i=0; i<this.tx_queue.length; i++)
		this.failed_sent_files.push({path:this.tx_queue[i].file.name, sendas:this.tx_queue[i].sendas});
	if ((!this.goteob) || this.tx_queue.length || this.pending_ack.length || this.pending_get.length)
		this.sendCmd(this.command.M_ERR, "Forced Shutdown");
	else {
		if (this.ver1_1) {
			if (this.senteob < 2)
				this.sendCmd(this.command.M_EOB);
			if (this.senteob < 2)
				this.sendCmd(this.command.M_EOB);
		}
		else {
			if (this.senteob < 1)
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
	if (this.debug || cmd == this.command.M_ERR) {
		if (cmd < this.command_name.length)
			type = this.command_name[cmd];
		else
			type = 'Unknown Command '+cmd;
		log(cmd == this.command.M_ERR ? LOG_NOTICE : LOG_DEBUG, "Sending "+type+" command args: "+data);
	}
	var len = data.length+1;
	len |= 0x8000;
	// We'll send it all in one go to avoid sending small packets...
	var sstr = this.send_buf(ascii((len & 0xff00)>>8) + ascii(len & 0xff) + ascii(cmd) + data);
	if (!this.send_chunks(sstr)) {
		log(LOG_WARNING, "Send failure");
		return false;
	}
	log(LOG_DEBUG, "Sent " + type + " command");
	switch(cmd) {
		case this.command.M_EOB:
			this.senteob++;
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
		case this.command.M_ADR:
		case this.command.M_PWD:
		case this.command.M_OK:
			break;
		default:
			this.reset_eob(false);
			break;
	}
	return true;
};
BinkP.prototype.sendData = function(data)
{
	var len = data.length;

	this.reset_eob(false);
	if (this.sock === undefined)
		return false;
	if (this.debug)
		log(LOG_DEBUG, "Sending "+data.length+" bytes of data");
	// We'll send it all in one go to avoid sending small packets...
	var sstr = this.send_buf(ascii((len & 0xff00)>>8) + ascii(len & 0xff) + data);
	if (!this.send_chunks(sstr)) {
		log(LOG_WARNING, "Send failure");
		return false;
	}
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
	var nullpos;
	var buf;

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
		i = this.sock.recv(1, timeout);
		if (i === null) {
			log(LOG_INFO, "Error in recv() of first byte of packet header");
			this.sock.close();
			this.sock = undefined;
			return undefined;
		}
		if (i.length != 1) {
			if (!this.sock.is_connected) {
				log(LOG_DEBUG, "Remote host closed socket");
				this.sock.close();
				this.sock = undefined;
				return undefined;
			}
			else if (timeout) {
				log(LOG_WARNING, "Timed out receiving first byte of packet header!");
				this.sock.close();
				this.sock = undefined;
				return undefined;
			}
			return null;
		}
		buf = i;
		i = this.sock.recv(1, this.timeout);
		if (i === null) {
			log(LOG_INFO, "Error in recv() of second byte of packet header");
			this.sock.close();
			this.sock = undefined;
			return undefined;
		}
		if (i.length != 1) {
			if (!this.sock.is_connected) {
				log(LOG_DEBUG, "Remote host closed socket");
				this.sock.close();
				this.sock = undefined;
				return undefined;
			}
			else if (timeout) {
				log(LOG_WARNING, "Timed out receiving second byte of packet header!");
				this.sock.close();
				this.sock = undefined;
				return undefined;
			}
			return null;
		}
		buf += i;
		buf = this.recv_buf(buf);
		ret.length = (ascii(buf[0]) << 8) | ascii(buf[1]);
		ret.is_cmd = (ret.length & 0x8000) ? true : false;
		ret.length &= 0x7fff;
		ret.data = '';
	}
	else
		ret = this.partialFrame;

	i = this.recv_buf(this.sock.recv(ret.length - ret.data.length, timeout));
	if (i == null) {
		log(LOG_INFO, "Error in recv() of packet data");
		this.sock.close();
		this.sock = undefined;
		return undefined;
	}
	if (i.length == 0) {
		if (!this.sock.is_connected) {
			log(LOG_DEBUG, "Remote host closed socket");
			this.sock.close();
			this.sock = undefined;
			return undefined;
		}
		else if (timeout) {
			log(LOG_WARNING, "Timed out receiving packet data from remote: " + this.remote_addrs);
			this.sock.close();
			this.sock = undefined;
			return undefined;
		}
	}
	ret.data += i;

	if (ret.data.length < ret.length)
		this.partialFrame = ret;
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
			nullpos = ret.data.indexOf(ascii(0));
			if (nullpos > -1)
				ret.data = ret.data.substr(0, nullpos);
			if (ret.command != this.command.M_EOB)
				this.reset_eob(false);
			switch(ret.command) {
				case this.command.M_ERR:
					log(LOG_ERROR, "BinkP got fatal error '"+ret.data+"' from remote: " + this.remote_addrs);
					this.sock.close();
					this.socket = undefined;
					return undefined;
				case this.command.M_BSY:
					log(LOG_WARNING, "BinkP got non-fatal error '"+ret.data+"' from remote: " + this.remote_addrs);
					this.sock.close();
					this.socket = undefined;
					return undefined;
				case this.command.M_EOB:
					this.goteob++;
					break;
				case this.command.M_ADR:
					if (this.remote_addrs !== undefined) {
						this.sendCmd(this.command.M_ERR, "Address already received.");
						return undefined;
					}
					else {
						this.remote_addrs = [];
						ret.data.split(/ /).forEach(function(addr) {
							try {
								this.remote_addrs.push(FIDO.parse_addr(addr, this.default_zone));
							}
							catch (e) {
							}
						}, this);
					}
					break;
				case this.command.M_OK:
					if (this.authenticated !== undefined) {
						this.sendCmd(this.command.M_ERR, "Authentication already complete.");
						return undefined;
					}
					else {
						log(LOG_INFO, "Authentication successful: " + ret.data);
						this.authenticated = ret.data;
					}
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
									switch(args[i]) {
										case 'NR':
											if (!this.sent_nr)
												this.sendCmd(this.command.M_NUL, "NR");
											this.nonreliable = true;
											break;
										case 'CRYPT':
											if (!this.wont_crypt) {
												this.will_crypt = true;
												log(LOG_INFO, "Will encrypt session.");
											}
											break;
									}
								}
							}
							break;
						case 'VER':
							log(LOG_INFO, "Peer version: " + args.slice(1).join(' '));
							tmp = ret.data.split(/ /);
							if (tmp.length >= 3) {
								this.remote_ver = tmp[1];
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
							break;
						case 'ZYZ':
							this.remote_operator = args.slice(1).join(' ');
							break;
						case 'NDL':
							this.remote_capabilities = args.slice(1).join(' ');
							break;
						default:
							this.remote_info[args[0]] = args.slice(1).join(' ');
							break;
					}
			}
		}
		else
			this.reset_eob(false);
	}
	return ret;
};
BinkP.prototype.addFile = function(path, sendas, waitget)
{
	var file = new File(path);

	this.reset_eob(true);
	if (sendas === undefined)
		sendas = file_getname(path);
	if (waitget === undefined)
		waitget = true;
	if (!file.open("rb", true)) {
		log(LOG_WARNING, "Error " + file.error + " opening '"+file.name+"'.  Not sending.");
		return false;
	}
	if (this.debug)
		log(LOG_DEBUG, "Adding '"+path+"' as '"+sendas+"'");
	this.tx_queue.push({file:file, sendas:sendas, waitingForGet:waitget});
	return true;
};
