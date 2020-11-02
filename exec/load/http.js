/* $Id: http.js,v 1.47 2020/07/22 04:31:48 echicken Exp $ */

require('sockdefs.js', 'SOCK_STREAM');
require('url.js', 'URL');

/*
 * TODO Stuff:
 *	Asynchronous requests
 *	Keep-alive support
 *	HTTP/1.1 (chunked, trailers, etc)
 *	Parse response headers
 */

function HTTPRequest(username,password,extra_headers)
{
	/* request properties */
	this.request_headers = undefined;
	this.referer = undefined;
	this.base = undefined;
	this.url = undefined;
	this.body = undefined;

	this.extra_headers = extra_headers;
	this.username=username;
	this.password=password;
	this.user_agent='SYNXv0.1';
	this.follow_redirects = 0;

	this.status = { ok: 200, created: 201, accepted: 202, no_content: 204 };
}

HTTPRequest.prototype.AddDefaultHeaders=function(){
	// General Headers
	this.request_headers.push("Connection: close");
	if(js.global.client != undefined)
		this.request_headers.push(
			"Via: "+client.protocol.toString().toLowerCase()+"/1.0 "+system.name);
	// Request Headers
	//this.request_headers.push("Accept: text/html,application/xhtml+xml,application/xml,text/*,*/*;q=0.9,*/*;q=0.8;q=0.7;q=0.6");
	this.request_headers.push("Accept: text/*,*/*;q=0.9");
	this.request_headers.push(
		"Accept-Charset: ISO-8859-13,Latin-9,ISO-8859-15,ISO-8859-1,UTF-8;q=0.5,*;q=0.1");
	this.request_headers.push("Accept-Encoding: ");
	this.request_headers.push("Host: "+this.url.host);
	if(this.referer != undefined)
		this.request_headers.push("Referer: "+this.referer);
	this.request_headers.push("User-Agent: "+this.user_agent);
};

HTTPRequest.prototype.AddExtraHeaders = function () {
	if (typeof this.extra_headers !== 'object') return;
	Object.keys(this.extra_headers).forEach(
		function (e) {
			this.request_headers.push(e + ': ' + this.extra_headers[e]);
		}, this
	);
};

HTTPRequest.prototype.SetupGet=function(url, referer, base) {
	this.referer=referer;
	this.base=base;
	this.url=new URL(url, this.base);
	if(this.url.scheme!='http' && this.url.scheme!='https')
		throw new Error("Unknown scheme! '"+this.url.scheme+"' in url:" + url);
	if(this.url.path=='')
		this.url.path='/';
	this.request="GET "+this.url.request_path+" HTTP/1.0";
	this.request_headers=[];
	this.AddDefaultHeaders();
	this.AddExtraHeaders();
};

HTTPRequest.prototype.SetupPost=function(url, referer, base, data, content_type) {
	if (content_type === undefined)
		content_type = 'application/x-www-form-urlencoded';
	this.referer=referer;
	this.base=base;
	this.url=new URL(url, this.base);
	if(this.url.scheme!='http' && this.url.scheme!='https')
		throw new Error("Unknown scheme! '"+this.url.scheme+"' in url: " + url);
	if(this.url.path=='')
		this.url.path='/';
	this.request="POST "+this.url.request_path+" HTTP/1.0";
	this.request_headers=[];
	this.AddDefaultHeaders();
	this.AddExtraHeaders();
	this.body=data;
	this.request_headers.push("Content-Type: "+content_type);
	this.request_headers.push("Content-Length: "+data.length);
};

HTTPRequest.prototype.SendRequest=function() {
	var i;
	var port;

	function do_send(sock, str) {
		var sent = 0;
		var ret;

		while (sent < str.length) {
			sock.poll(60, true);
			ret = sock.send(str);
			if (ret === 0 || ret === null || ret === false)
				return false;
			if (ret === true)
				return true;
			sent += ret;
		}
		return true;
	}

	if (this.sock != undefined)
		this.sock.close();
	port = this.url.port?this.url.port:(this.url.scheme=='http'?80:443);
	if (js.global.ConnectedSocket != undefined) {
		if ((this.sock = new ConnectedSocket(this.url.host, port)) == null)
			throw new Error(format("Unable to connect to %s:%u", this.url.host, this.url.port));
	}
	else {
		if((this.sock=new Socket(SOCK_STREAM))==null)
			throw new Error("Unable to create socket");
		if(!this.sock.connect(this.url.host, port)) {
			this.sock.close();
			throw new Error(format("Unable to connect to %s:%u", this.url.host, this.url.port));
		}
	}
	if(this.url.scheme=='https')
		this.sock.ssl_session=true;
	if(!do_send(this.sock, this.request+"\r\n"))
		throw new Error("Unable to send request: " + this.request);
	for(i in this.request_headers) {
		if(!do_send(this.sock, this.request_headers[i]+"\r\n"))
			throw new Error("Unable to send headers");
	}
	if(!do_send(this.sock, "\r\n"))
		throw new Error("Unable to terminate headers");
	if(this.body != undefined) {
		if(!do_send(this.sock, this.body))
			throw new Error("Unable to send body");
	}
};

HTTPRequest.prototype.ReadStatus=function() {
	this.status_line=this.sock.recvline(4096);
	if(this.status_line==null)
		throw new Error("Unable to read status");
	var m = this.status_line.match(/^HTTP\/[0-9]+\.[0-9]+ ([0-9]{3})/);
	if (m === null)
		throw new Error("Unable to parse status line '"+this.status_line+"'");
	this.response_code = parseInt(m[1], 10);
};

HTTPRequest.prototype.ReadHeaders=function() {
	var header='';
	var m;
	this.response_headers=[];
	this.response_headers_parsed={};

	for(;;) {
		header=this.sock.recvline(4096, 120);
		if(header==null)
			throw new Error("Unable to receive headers");
		if(header=='')
			return;
		this.response_headers.push(header);
		m=header.match(/^Content-length:\s+([0-9]+)$/i);
		if(m!=null)
			this.contentlength=parseInt(m[1]);
		m = header.match(/^(.*?):\s*(.*?)\s*$/);
		if (m) {
			if (this.response_headers_parsed[m[1]] == undefined)
				this.response_headers_parsed[m[1]] = [];
			this.response_headers_parsed[m[1]].push(m[2]);
		}
	}
};

HTTPRequest.prototype.ReadBody=function() {
	var ch;
	var lastlen=0;
	var len=this.contentlength;
	if(len==undefined)
		len=1024;

	this.body='';
	while((ch=this.sock.recv(len))!=null && ch != '') {
		this.body += ch.toString();
		len -= ch.length;
		if(len < 1)
			len=1024;
		js.flatten_string(this.body);
	}
};

HTTPRequest.prototype.ReadResponse=function() {
	this.ReadStatus();
	this.ReadHeaders();
	this.ReadBody();
};

HTTPRequest.prototype.BasicAuth=function(username,password) {
	if(username && password) {
		this.username=username;
		this.password=password;
	}
	if(this.username && this.password) {
		var auth = base64_encode(this.username + ":" + this.password);
		this.request_headers.push("Authorization: Basic " + auth);
	}
};

HTTPRequest.prototype.Get=function(url, referer, base) {
	this.SetupGet(url,referer,base);
	this.BasicAuth();
	this.SendRequest();
	this.ReadResponse();
	if ([301, 302, 307, 308].indexOf(this.response_code) > -1
		&& this.follow_redirects > 0
		&& this.response_headers_parsed.Location
		&& this.response_headers_parsed.Location.length
	) {
		this.follow_redirects--;
		return this.Get(this.response_headers_parsed.Location[0], url); // To-do: be less tired and think about referer,base
	}
	return(this.body);
};

HTTPRequest.prototype.Post=function(url, data, referer, base, content_type) {
	this.SetupPost(url,referer,base,data, content_type);
	this.BasicAuth();
	this.SendRequest();
	this.ReadResponse();
	return(this.body);
};

HTTPRequest.prototype.Head=function(url, referer, base) {
	var i;
	var m;
	var ret={};

	this.SetupGet(url,referer,base);
	this.request = this.request.replace(/^GET/, 'HEAD');
	this.BasicAuth();
	this.SendRequest();
	this.ReadResponse();
	for(i in this.response_headers) {
		m = this.response_headers[i].match(/^(.*?):\s*(.*?)\s*$/);
		if (m) {
			if (ret[m[1]] == undefined)
				ret[m[1]] = [];
			ret[m[1]].push(m[2]);
		}
	}
	return(ret);
};
