/* $Id: ftp.js,v 1.23 2020/05/21 01:56:45 rswindell Exp $ */

require('sockdefs.js', 'SOCK_STREAM');

function FTP(host, user, pass, port, dport, bindhost, account)
{
	var ret;

	if (host === undefined)
		throw("No hostname specified");
	
	this.revision = "JSFTP v" + "$Revision: 1.23 $".split(' ')[1];

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
	var response = this.cmd(undefined, true);
	if (parseInt(response, 10) === 120) {
		log(LOG_NOTICE, "Connection delay: "+response);
		response = this.cmd(undefined, true);
	}
	if (parseInt(response, 10) !== 220) {
		this.socket.close();
		throw("Invalid response from server: " + response);
	}
	ret = parseInt(response = this.cmd("USER "+this.user, true), 10)
	if (ret === 331)
		ret = parseInt(response = this.cmd("PASS "+this.pass, true), 10);
	if (ret === 332) {
		if (this.account === undefined)
			throw("Account required");
		ret = parseInt(response = this.cmd("ACCT "+this.account, true), 10);
	}
	if (ret !== 230 && ret != 202) {
		this.socket.close();
		throw("Login failed: " + response);
	}
	this.ascii = false;
	this.passive = true;
}

FTP.prototype.allo = function(size, pagesize)
{
	var cmd = "ALLO "+size;
	if (pagesize !== undefined)
		cmd += ' '+pagesize;
	switch(parseInt(this.cmd(cmd, true))) {
		case 200:
		case 202:
			return true;
	}
	return false;
}
FTP.prototype.allocate = FTP.prototype.allo;

FTP.prototype.appe = function(src, dest)
{
	var data_socket;

	data_socket = this.data_socket("APPE "+dest)

	return this.do_sendfile(src, data_socket);
}
FTP.prototype.append = FTP.prototype.appe;

FTP.prototype.cdup = function()
{
	var rstr;
	var ret;

	rstr = this.cmd("CDUP", true);
	ret = parseInt(rstr, 10);
	if (ret !== 200)
		return false;
	return true;
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

FTP.prototype.dele = function(path)
{
	if (parseInt(this.cmd("DELE "+path, true), 10) !== 250)
		return false;
	return true;
}
FTP.prototype.delete = FTP.prototype.dele;

FTP.prototype.list = function(path)
{
	return this.do_get("LIST "+path);
}
FTP.prototype.dir = FTP.prototype.list;

FTP.prototype.help = function(cmd)
{
	var ret;

	if (cmd === undefined)
		ret = this.cmd("HELP", true);
	else
		ret = this.cmd("HELP "+cmd, true);
	switch(parseInt(ret, 10)) {
		case 211:
		case 214:
			return ret.replace(/^[0-9]+[ -]/mg,'');;
	}
	return null;
}

// TODO: Not tested
FTP.prototype.mkd = function(path)
{
	if (parseInt(this.cmd("MKD "+path, true), 10) !== 257)
		return false;
	return true;
}
FTP.prototype.make_directory = FTP.prototype.mkd;

FTP.prototype.nlst = function(path)
{
	return this.do_get("NLST "+path);
}
FTP.prototype.name_list = FTP.prototype.nlst;

FTP.prototype.pwd = function()
{
	var rstr;
	var ret;

	rstr = this.cmd("PWD", true);
	ret = parseInt(rstr, 10);
	if (ret === 257)
		return rstr.replace(/^257 "(.*)".*?$/, "$1");
	return null;
}

FTP.prototype.quit = function()
{
	var ret;

	ret = parseInt(this.cmd("QUIT", true), 10)
	this.socket.close();
	if (ret !== 221)
		return false;
	return true;
}
FTP.prototype.logout = FTP.prototype.quit

FTP.prototype.retr = function(src, dest)
{
	return this.do_get("RETR "+src, dest);
}
FTP.prototype.get = FTP.prototype.retr;
FTP.prototype.retrieve = FTP.prototype.retr;

// TODO: Not tested
FTP.prototype.rmd = function(path)
{
	if (parseInt(this.cmd("RMD "+path, true), 10) !== 250)
		return false;
	return true;
}
FTP.prototype.remove_directory = FTP.prototype.rmd;

// TODO: untested.
FTP.prototype.rename = function(from, to)
{
	switch(parseInt(this.cmd("RNFR "+from, true), 10)) {
		case 350:
			break;
		default:
			return false;
	}
	if(parseInt(this.cmd("RNTO "+from, true), 10) !== 250)
		return false;
	return true;
}

// TODO: Not tested
FTP.prototype.site = function(str)
{
	if (parseInt(this.cmd("SITE "+str, true), 10) !== 200)
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

FTP.prototype.stor = function(src, dest)
{
	var data_socket;

	data_socket = this.data_socket("STOR "+dest);

	return this.do_sendfile(src, data_socket);
}
FTP.prototype.put = FTP.prototype.stor;
FTP.prototype.store = FTP.prototype.stor;

/*
 * TODO: Untested, Synchronet doesn't support it.
 * This *should* parse and return the file name
 */
FTP.prototype.stou = function(src)
{
	var data_socket;
	var m;

	data_socket = this.data_socket("STOU");

	if (!this.do_sendfile(src, data_socket))
		return null;
	m = data_socket.ftp_response.match(/^250 .*?"(.*)".*?$/);
	if (m === null)
		return null;
	return m[1];
}
FTP.prototype.store_unique = FTP.prototype.stou;

FTP.prototype.stat = function(path)
{
	var ret;

	if (path === undefined)
		ret = this.cmd("STAT", true);
	else
		ret = this.cmd("STAT "+path, true);
	switch(parseInt(ret, 10)) {
		case 211:
		case 212:
		case 213:
			return ret.replace(/^[0-9]+[ -]/mg,'');;
	}
	return null;
}
FTP.prototype.status = FTP.prototype.stat;

FTP.prototype.syst = function()
{
	var ret;

	ret = this.cmd("SYST", true);
	if (parseInt(ret, 10) !== 215)
		return null;
	return ret.replace(/^[0-9]+[ -]/mg,'');;
}
FTP.prototype.system = FTP.prototype.syst;

FTP.prototype.do_sendfile = function(src, data_socket)
{
	var rstr;
	var tmp_socket;
	var selret;
	var f;
	var buf;
	var total = 0;
	var error = false;

	var f = new File(src);
	if (!f.open("rb")) {
		data_socket.close();
		throw("Error " + f.error + " opening file '" + f.name + "'");
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

	rstr = this.cmd(undefined, true);
	if (parseInt(rstr, 10) !== 226) {
		throw("Data connection not closed: "+rstr);
	}
	if (!error)
		log(LOG_DEBUG, "Sent "+total+" bytes.");
	return !error;
}

FTP.prototype.cmd = function(cmd, needresp)
{
	var cmdline = '';
	var start;
	var rd;
	var ret = '';
	var rsp;
	var m;
	var done = false;

	if (!this.socket.is_connected)
		throw("Socket disconnected");

	if (cmd !== undefined) {
		while (this.socket.data_waiting) {
			start = time();
			log(LOG_ERROR, "Error: Unexpected data on control connection: "+this.socket.recvline(this.maxline, this.timeout - (time() - start)));
		}
		cmdline = cmd.replace(/\xff/g, "\xff\xff") + '\r\n';
		log(LOG_DEBUG, "CMD: '"+cmd+"'");
		if (this.socket.send(cmdline) != cmdline.length)
			throw("Error " + this.socket.error + " sending command: '" + cmd + "'");
	}

	if (needresp === true) {
		start = time();

		do {
			rd = this.socket.recvline(this.maxline, this.timeout - (time() - start));
			if (rd !== null) {
				m = rd.match(/^([0-9]{3})([- ])/);
				if (rsp === undefined) {
					if (m === null) {
						throw("Invalid response: "+rd);
					}
					rsp = m[1];
					if (m[2] === ' ')
						done = true;
				}
				else if(m !== null) {
					if (m[1] === rsp && m[2] === ' ')
						done = true;
				}
				rd = rd.replace(/\xff\xff/g, "\xff");
				log(LOG_DEBUG, "RSP: '"+rd+"'");
				ret += rd + "\r\n";
				if (rd.length === 0)
					continue;
			}
			else {
				if(cmd)
					throw("recvline timeout waiting for response to command: '" + cmd + "'");
				else
					throw("recvline timeout waiting for additional response");
			}
		} while(this.socket.is_connected && !done);
		return ret;
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
	var ip6;
	var rhost;
	var rport;

	if (this.ascii === true)
		rstr = this.cmd("TYPE A", true);
	else
		rstr = this.cmd("TYPE I", true);
	if (parseInt(rstr, 10) !== 200)
		throw("Unable to create data socket: " + rstr);

	ip6 = this.socket.local_ip_address.indexOf(':') !== -1;
	if (this.passive) {
		// TODO: Outgoing port?
		if (ip6) {
			rstr = this.cmd("EPSV", true);
			if (parseInt(rstr, 10) !== 229)
				throw("EPSV Failed: " + rstr);
			m = rstr.match(/\(\|\|\|([0-9]+)\|\)/);
			if (m === null)
				throw("Unable to parse EPSV reply: " + rstr);
			rhost = this.host;
			rport = parseInt(m[1], 10);
		}
		else {
			rstr = this.cmd("PASV", true);
			if (parseInt(rstr, 10) !== 227)
				throw("PASV Failed: " + rstr);
			m = rstr.match(/\(([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+),([0-9]+)\)/);
			if (m === null)
				throw("Unable to parse PASV reply: " + rstr);
			rhost = m[1] + '.' + m[2] + '.' + m[3] + '.' + m[4];
			rport = (parseInt(m[5], 10) << 8) | parseInt(m[6], 10);
		}
		ds = new ConnectedSocket(rhost, rport, {protocol:'FTP-Data', timeout:this.timeout, binadaddrs:this.bindhost});
	}
	else {
		// TODO: No way to check if IPv6...
		ds = new Socket(SOCK_STREAM, "FTP-Data", (ip6));
		ds.bind(this.dport, this.socket.local_ip_address);
		ds.listen();
		try {
			if (ip6) {
				rstr = this.cmd("EPRT |" + (ds.local_ip_address.indexOf(':') === -1 ? '1' : '2') + "|" + ds.local_ip_address + "|" + ds.local_port + "|", true);
			}
			else {
				var addrport = this.socket.local_ip_address.split(/\./);
				addrport.push(ds.local_port >> 8);
				addrport.push(ds.local_port & 0xff);
				rstr = this.cmd("PORT "+addrport.join(",")+"", true);
			}
		} catch(e) {
			ds.close();
			throw(e);
		}
		if (parseInt(rstr, 10) !== 200) {
			ds.close();
			throw("EPRT/PORT rejected: " + rstr);
		}
	}

	rstr = this.cmd(cmd, true);
	switch (parseInt(rstr, 10)) {
		case 150:
			break;
		case 125:
			if (ds.is_connected)
				break;
			// Fall-through
		default:
			ds.close();
			throw(cmd+" failed: " + rstr);
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

	ds.ftp_response = rstr;
	return ds;
}

FTP.prototype.do_get = function(cmd, dest)
{
	var rstr;
	var data_socket;
	var tmp_socket;
	var selret;
	var f;
	var ret = '';
	var rbuf;
	var total = 0;

	data_socket = this.data_socket(cmd)

	if (dest !== undefined) {
		f = new File(dest);
		if (!f.open("wb")) {
			data_socket.close();
			throw("Error " + f.error + " opening file '" + f.name + "'");
		}
	}

	do {
		rbuf = data_socket.recv(4096, this.timeout);
		if (rbuf !== null) {
			total += rbuf.length;
			if (dest === undefined)
				ret += rbuf;
			else
				f.write(rbuf);
		}
		else {
			throw("recv timeout");
		}
	} while(data_socket.is_connected && this.socket.is_connected);
	data_socket.close();
	if (f !== undefined)
		f.close();
	if (parseInt(this.cmd(undefined, true), 10) !== 226)
		throw("Data connection not closed");
	log(LOG_DEBUG, "Received "+total+" bytes.");
	if (dest === undefined)
		return ret;
	return true;
}
