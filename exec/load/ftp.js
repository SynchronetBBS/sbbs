/* $Id$ */

require('sockdefs.js', 'SOCK_STREAM');

function FTP(host, user, pass, port, dport, bindhost, account)
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
	this.account = account;
	this.socket = new ConnectedSocket(host, port, {protocol:'FTP', timeout:this.timeout, binadaddrs:this.bindhost});
	if (parseInt(this.cmd(undefined, true), 10) !== 220) {
		this.socket.close();
		throw("Invalid response from server");
	}
	ret = parseInt(this.cmd("USER "+this.user, true), 10)
	if (ret === 331)
		ret = parseInt(this.cmd("PASS "+this.pass, true), 10);
	if (ret === 332) {
		if (this.account === undefined)
			throw("Account required");
		ret = parseInt(this.cmd("ACCOUNT "+this.account, true), 10);
	}
	if (ret !== 230 && ret != 202) {
		this.socket.close();
		throw("Login failed");
	}
	this.ascii = false;
	this.passive = true;
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

FTP.prototype.cdup = function()
{
	var rstr;
	var ret;

	rstr = this.cmd("CDUP");
	ret = parseInt(rstr, 10);
	if (ret !== 200)
		return false;
	return true;
}

FTP.prototype.smnt = function(path)
{
	var rstr;
	var ret;

	rstr = this.cmd("SMNT");
	ret = parseInt(rstr, 10);
	if (ret !== 250)
		return false;
	return true;
}

FTP.prototype.logout = function()
{
	var ret;

	ret = parseInt(this.cmd("QUIT", true), 10)
	this.socket.close();
	if (ret !== 221)
		return false;
	return true;
}

// REIN... not implemented

FTP.prototype.pwd = function()
{
	var rstr;
	var ret;

	rstr = this.cmd("PWD", true);
	ret = parseInt(rstr, 10);
	if (ret === 257)
		return rstr.replace(/^257 "(.*)".*?$/, '$1');
	return null;
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
	var data_socket;
	var tmp_socket;
	var selret;
	var f;
	var buf;
	var total = 0;
	var error = false;

	data_socket = this.data_socket("STOR "+dest)

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
		log(LOG_DEBUG, "CMD: '"+cmd.replace(/\xff/g, "\xff\xff")+"'");
		if (this.socket.send(cmdline) != cmdline.length)
			throw("Error sending command");
	}

	if (needresp === true) {
		start = time();

		do {
			rd = this.socket.recvline(this.maxline, this.timeout - (time() - start));
			if (rd !== null) {
				rd = rd.replace(/\xff\xff/g, "\xff");
				log(LOG_DEBUG, "RSP: '"+rd+"'");
				if (rd.length === 0)
					continue;
			}
		} while(this.socket.is_connected && (rd[0] === ' ' || rd[3] === '-'));
		return rd;
	}
	return null;
}

FTP.prototype.data_socket = function(cmd)
{
	var rstr;
	var ds;
	var ts;
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

	rstr = this.cmd(cmd, true);
	if (parseInt(rstr, 10) !== 150) {
		ds.close();
		throw(cmd+" failed");
	}
	if (!this.passive) {
		selret = socket_select([ds], this.timeout);
		if (selret === null || selret.length === 0) {
			ds.close();
			throw("Timeout waiting for remote to connect");
		}
		ts = ds.accept();
		ds.close();
		ds = ts;
	}

	return ds;
}

FTP.prototype.do_get = function(src, dest, isdir)
{
	var rstr;
	var data_socket;
	var tmp_socket;
	var selret;
	var f;
	var ret = '';
	var rbuf;
	var total = 0;

	data_socket = this.data_socket(isdir ? ("LIST "+src) : ("RETR "+src))

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

var f = new FTP('fd0b:71d1:b5ec::1');
f.passive = false;
f.cwd("main");
print(f.pwd());
print(f.dir('.'));
f.logout();
