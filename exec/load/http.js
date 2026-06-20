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

function HTTPRequest(username,password,extra_headers,recv_timeout)
{
	/* request properties */
	this.request_headers = undefined;
	this.referer = undefined;
	this.base = undefined;
	this.url = undefined;
	this.body = undefined;
	this.output_file = undefined;	/* set to a File (opened "wb") to stream the response body to disk instead of buffering it in this.body */
	this.body_length = 0;		/* bytes received for the last response body (to string or file) */

	this.extra_headers = extra_headers;
	this.username=username;
	this.password=password;
	this.user_agent='SYNXv0.1';
	this.follow_redirects = 0;
	this.recv_timeout = recv_timeout || 60;

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
	this.status_line=this.sock.recvline(4096, this.recv_timeout);
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
		header=this.sock.recvline(4096, this.recv_timeout);
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
			var lc = m[1].toLowerCase();
			if (lc !== m[1]) {
				if (this.response_headers_parsed[lc] == undefined)
					this.response_headers_parsed[lc] = [];
				this.response_headers_parsed[lc].push(m[2]);
			}
		}
	}
};

HTTPRequest.prototype.ReadBody=function() {
	var ch;
	var len=this.contentlength;
	if(len==undefined)
		len=1024;

	/* Stream the body straight to a file when output_file is set, instead of
	 * buffering the whole download in a string (costly/risky for large files
	 * under SpiderMonkey 1.8.5). Only for a final success (2xx) response: a
	 * redirect (3xx) body is discarded so Get() can re-request, and a 4xx/5xx
	 * error body stays in this.body for the caller to inspect. Reads in bounded
	 * chunks so a huge Content-Length doesn't size a single recv(). */
	if(this.output_file != undefined
		&& this.response_code >= 200 && this.response_code < 300) {
		this.body = undefined;
		this.body_length = 0;
		while((ch=this.sock.recv(16384, this.recv_timeout))!=null && ch != '') {
			if(!this.output_file.write(ch))
				throw new Error("Error writing to file: " + this.output_file.name);
			this.body_length += ch.length;
		}
		return;
	}

	this.body='';
	while((ch=this.sock.recv(len, this.recv_timeout))!=null && ch != '') {
		this.body += ch.toString();
		len -= ch.length;
		if(len < 1)
			len=1024;
		js.flatten_string(this.body);
	}
	this.body_length = this.body.length;
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
		&& this.response_headers_parsed.location
		&& this.response_headers_parsed.location.length
	) {
		this.follow_redirects--;
		return this.Get(this.response_headers_parsed.location[0], this.url.url, this.url.url);
	}
	return(this.body);
};

/* Download a URL straight to a file, streaming the body to disk rather than
 * buffering it in memory -- suitable for large files. Opens `filename` in binary
 * mode, follows redirects (set .follow_redirects first if needed), and closes the
 * file before returning. Returns the number of body bytes written to the file.
 * The HTTP status is in .response_code -- check it for 200: a non-2xx response
 * writes no file data and leaves the (error/redirect) body in .body. Throws on a
 * socket or file error. */
HTTPRequest.prototype.Download=function(url, filename, referer, base) {
	var f = new File(filename);
	if(!f.open("wb"))
		throw new Error("Unable to open file for writing: " + filename);
	this.output_file = f;
	try {
		this.Get(url, referer, base);
	} finally {
		f.close();
		this.output_file = undefined;
	}
	return (this.response_code >= 200 && this.response_code < 300) ? this.body_length : 0;
};

HTTPRequest.prototype.Post=function(url, data, referer, base, content_type) {
	this.SetupPost(url,referer,base,data, content_type);
	this.BasicAuth();
	this.SendRequest();
	this.ReadResponse();
	return(this.body);
};

/* Streaming POST: same shape as Post(), but reads the response in chunks
 * and invokes `on_chunk(text)` for each body chunk as it arrives. Returns
 * the full accumulated body string after the stream completes.
 *
 * Handles HTTP/1.1 chunked transfer encoding, which is what server-sent
 * events (SSE) and similar streaming APIs use. Falls back to a single
 * on_chunk call with the whole body if the server doesn't chunk.
 *
 * Sends HTTP/1.1 (vs the default Post()'s 1.0) because chunked transfer
 * is an HTTP/1.1 feature -- servers using non-chunked frames over 1.0
 * still work but won't actually stream incrementally. */
HTTPRequest.prototype.SetupPostStreaming = function(url, referer, base, data, content_type) {
	if (content_type === undefined)
		content_type = 'application/x-www-form-urlencoded';
	this.referer = referer;
	this.base    = base;
	this.url     = new URL(url, this.base);
	if (this.url.scheme != 'http' && this.url.scheme != 'https')
		throw new Error("Unknown scheme! '" + this.url.scheme + "' in url: " + url);
	if (this.url.path == '')
		this.url.path = '/';
	this.request = "POST " + this.url.request_path + " HTTP/1.1";
	this.request_headers = [];
	this.AddDefaultHeaders();
	this.AddExtraHeaders();
	this.body = data;
	this.request_headers.push("Content-Type: " + content_type);
	this.request_headers.push("Content-Length: " + data.length);
};

/* Read a chunked-transfer-encoded body, calling on_chunk(text) for each
 * chunk as it arrives. Decodes the wire format:
 *     <hex_size>\r\n   (optional ";chunk-extension" after the size)
 *     <bytes>\r\n
 *     ...repeats...
 *     0\r\n
 *     \r\n              (optional trailers, then empty line)
 * Sets this.body to the accumulated text. */
HTTPRequest.prototype.ReadChunkedBody = function(on_chunk) {
	var accum = '';
	while (true) {
		var size_line = this.sock.recvline(64, this.recv_timeout);
		if (size_line == null)
			throw new Error("Unable to read chunk size");
		/* Chunk-size lines can have ";extension" after hex size. */
		var hex = size_line.replace(/[\r\n;].*$/, '').replace(/^\s+|\s+$/g, '');
		var chunk_size = parseInt(hex, 16);
		if (isNaN(chunk_size))
			throw new Error("Bad chunk size: '" + size_line + "'");
		if (chunk_size == 0) {
			/* End of stream: read trailing CRLF (or trailer headers) */
			while (true) {
				var trailer = this.sock.recvline(4096, this.recv_timeout);
				if (trailer == null || trailer == '') break;
			}
			break;
		}
		/* Read exactly chunk_size bytes. */
		var remaining = chunk_size;
		var chunk = '';
		while (remaining > 0) {
			var part = this.sock.recv(remaining, this.recv_timeout);
			if (part == null || part == '')
				throw new Error("Unable to read chunk data");
			var s = part.toString();
			chunk += s;
			remaining -= s.length;
		}
		/* Consume the trailing CRLF after the chunk data. */
		this.sock.recvline(4, this.recv_timeout);

		accum += chunk;
		if (on_chunk)
			on_chunk(chunk);
	}
	this.body = accum;
};

HTTPRequest.prototype.PostStreaming = function(url, data, on_chunk, content_type, referer, base) {
	this.SetupPostStreaming(url, referer, base, data, content_type);
	this.BasicAuth();
	this.SendRequest();
	this.ReadStatus();
	this.ReadHeaders();
	var te_list = this.response_headers_parsed['transfer-encoding']
	           || this.response_headers_parsed['Transfer-Encoding'];
	var te = (te_list && te_list.length) ? te_list.join(' ').toLowerCase() : '';
	if (te.indexOf('chunked') >= 0) {
		this.ReadChunkedBody(on_chunk);
	} else {
		/* Non-chunked response: read the whole body, deliver as a single
		 * chunk. Caller still gets the on_chunk callback so they don't
		 * need separate code paths. */
		this.ReadBody();
		if (on_chunk && this.body)
			on_chunk(this.body);
	}
	return this.body;
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
