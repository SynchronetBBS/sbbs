/* $Id$ */

require('sockdefs.js', 'SOCK_STREAM');

function FTP(host, user, pass, port, dport, bindhost)
{
	var ret;

	if (host === undefined)
		throw("No hostname specified");

	if (port === undefined)
		port = 21;
	if (dport === undefined)
		dport = 0;
	if (user === undefined)
		user = 'anonymous';
	if (pass === undefined)
		pass = 'sysop@' + system.host_name;
	if (bindhost === undefined)
		bindhost = system.hostname;
	if (bindhost === undefined)
		bindhost = ['0.0.0.0', '::'];

	this.host = host;
	this.user = user;
	this.pass = pass;
	this.dport = dport;
	this.bindhost = bindhost;
	this.timeout = 300;
	this.maxline = 500;
	this.socket = new ConnectedSocket(host, port, {protocol:'FTP', timeout:this.timeout, binadaddrs:this.bindhost});
	if (parseInt(this.cmd(undefined, true), 10) !== 220) {
		this.socket.close();
		throw("Invalid response from server");
	}
	ret = parseInt(this.cmd("USER "+this.user, true), 10)
	if (ret === 331)
		ret = parseInt(this.cmd("PASS "+this.pass, true), 10);
	if (ret !== 230) {
		this.socket.close();
		throw("Login failed");
	}
	this.ascii = false;
	this.passive = true;
}

FTP.prototype.logout = function()
{
	var ret;

	ret = parseInt(this.cmd("QUIT", true), 10)
	// TODO: Hrm....
	if (ret !== 221)
		return false;
	this.socket.close();
	return true;
}

FTP.prototype.pwd = function()
{
	var rstr;
	var ret;

	rstr = this.cmd("PWD", true);
	ret = parseInt(rstr, 10);
	if (ret === 257)
		return rstr.replace(/^[0-9]+ /, '');
	return null;
}

FTP.prototype.cwd = function(path)
{
	var rstr;
	var ret;

	rstr = this.cmd("CWD "+path, true);
	ret = parseInt(rstr, 10);
	if (ret !== 250)
		return false;
	return true;
}

FTP.prototype.dir = function(path)
{
	return this.do_get(path, undefined, true);
}

FTP.prototype.delete = function(path)
{
	if (parseInt(this.cmd("DELE "+path, true), 10) !== 250)
		return false;
	return true;
}

FTP.prototype.get = function(src, dest)
{
	return this.do_get(src, dest, false);
}

FTP.prototype.put = function(src, dest)
{
	var rstr;
	var data_socket = this.data_socket();
	var tmp_socket;
	var selret;
	var f;
	var buf;
	var total = 0;
	var error = false;

	rstr = this.cmd("STOR "+dest, true);
	if (parseInt(rstr, 10) !== 150) {
		data_socket.close();
		throw("PUT failed");
	}
	if (!this.passive) {
		selret = socket_select([data_socket], this.timeout);
		if (selret === null || selret.length === 0) {
			data_socket.close();
			throw("Timeout waiting for remote to connect");
		}
		tmp_socket = data_socket.accept();
		data_socket.close();
		data_socket = tmp_socket;
	}

	f = new File(src);
	if (!f.open("rb")) {
		data_socket.close();
		throw("Error opening file '"+f.name+"'");
	}

	do {
		buf = f.read(4096);
		if (data_socket.send(buf) !== buf.length) {
			error = true;
			break;
		}
		total += buf.length;
		
	} while((!f.eof) && data_socket.is_connected && this.socket.is_connected);
	data_socket.close();
	f.close();
	if (parseInt(this.cmd(undefined, true), 10) !== 226)
		return false;
	if (!error)
		log(LOG_DEBUG, "Sent "+total+" bytes.");
	return !error;
}

FTP.prototype.cmd = function(cmd, needresp)
{
	var cmdline = '';
	var start;
	var rd;

	if (!this.socket.is_connected)
		throw("Socket disconnected");

	if (cmd !== undefined) {
		cmdline = cmd + '\r\n';
		log(LOG_DEBUG, "CMD: '"+cmd+"'");
		if (this.socket.send(cmdline) != cmdline.length)
			throw("Error sending command");
	}

	if (needresp === true) {
		start = time();

		do {
			rd = this.socket.recvline(this.maxline, this.timeout - (time() - start));
			if (rd !== null) {
				log(LOG_DEBUG, "RSP: '"+rd+"'");
				if (rd.length === 0)
					continue;
			}
		} while(this.socket.is_connected && (rd[0] === ' ' || rd[3] === '-'));
		return rd;
	}
	return null;
}

FTP.prototype.data_socket = function()
{
	var rstr;
	var ds;
	var m;
	var splitaddr;

	if (this.ascii === true)
		rstr = this.cmd("TYPE A", true);
	else
		rstr = this.cmd("TYPE I", true);
	if (parseInt(rstr, 10) !== 200)
		throw("Unable to create data socket");
	if (this.passive) {
		// TODO: Outgoing port?
		rstr = this.cmd("EPSV", true);
		if (parseInt(rstr, 10) !== 229)
			throw("EPSV Failed");
		m = rstr.match(/\(\|\|\|([0-9]+)\|\)/);
		if (m === null)
			throw("Unable to parse EPSV reply");
		return new ConnectedSocket(this.host, parseInt(m[1], 10), {protocol:'FTP-Data', timeout:this.timeout, binadaddrs:this.bindhost});
	}

	// TODO: No way to check if IPv6...
	ds = new Socket(SOCK_STREAM, "FTP-Data", (this.socket.local_ip_address.indexOf(':') !== -1));
	ds.bind(this.dport, this.socket.local_ip_address);
	ds.listen();
	try {
		rstr = this.cmd("EPRT |" + (ds.local_ip_address.indexOf(':') === -1 ? '1' : '2') + "|" + ds.local_ip_address + "|" + ds.local_port + "|", true);
	} catch(e) {
		ds.close();
		throw(e);
	}
	if (parseInt(rstr, 10) !== 200) {
		ds.close();
		throw("EPRT rejected");
	}
	return ds;
}

FTP.prototype.do_get = function(src, dest, isdir)
{
	var rstr;
	var data_socket = this.data_socket();
	var tmp_socket;
	var selret;
	var f;
	var ret = '';
	var rbuf;
	var total = 0;

	if (isdir)
		rstr = this.cmd("LIST "+src, true);
	else
		rstr = this.cmd("RETR "+src, true);
	if (parseInt(rstr, 10) !== 150) {
		data_socket.close();
		throw("GET failed");
	}
	if (!this.passive) {
		selret = socket_select([data_socket], this.timeout);
		if (selret === null || selret.length === 0) {
			data_socket.close();
			throw("Timeout waiting for remote to connect");
		}
		tmp_socket = data_socket.accept();
		data_socket.close();
		data_socket = tmp_socket;
	}

	if (!isdir) {
		f = new File(dest);
		if (!f.open("wb")) {
			data_socket.close();
			throw("Error opening file '"+f.name+"'");
		}
	}

	do {
		rbuf = data_socket.recv(4096, this.timeout);
		if (rbuf !== null) {
			total += rbuf.length;
			if (isdir)
				ret += rbuf;
			else
				f.write(rbuf);
		}
	} while(data_socket.is_connected && this.socket.is_connected);
	data_socket.close();
	if (f !== undefined)
		f.close();
	log(LOG_DEBUG, "Received "+total+" bytes.");
	if (isdir)
		return ret;
	return true;
}
