/*
 * http://spamassassin.apache.org/full/3.0.x/dist/spamd/PROTOCOL
 * $Id: salib.js,v 1.28 2016/04/24 00:55:45 deuce Exp $
 */

require('sockdefs.js', 'SOCK_STREAM');

function SPAMC_Message(messagefile, addr, port, user)
{
	if(!file_exists(messagefile))
		this.error="Message file '"+messagefile+"' does not exist";
	this.addr=addr;
	if(this.addr==undefined)
		this.addr='127.0.0.1';
	this.port=port;
	if(this.port==undefined)
		this.port='783';
	this.user=user;
	this.messagefile=messagefile;
	this.DoCommand=Message_DoCommand;
	this.check =function() { return(this.DoCommand('CHECK')); };
	this.symbols =function() { return(this.DoCommand('SYMBOLS')); };
	this.report =function() { return(this.DoCommand('REPORT')); };
	this.report_ifspam =function() { return(this.DoCommand('REPORT_IFSPAM')); };
	this.process =function() { return(this.DoCommand('PROCESS')); };
}

function Message_DoCommand(command)
{
	var rcvd=new Array();
	var tmp;
	var sock=new Socket(SOCK_STREAM, "spamc");
	var ret={message:'', isSpam:false, score:'unknown', threshold:'unknown', symbols:[]};
	var	inserted_header_fields="";

	if(!sock.connect(this.addr, this.port)) {
		sock.close();
		ret.error='Failed to connect to spamd';
		return(ret);
	}

	var content_length = file_size(this.messagefile);

	if(this.reverse_path)
		inserted_header_fields += "Return-Path: " + this.reverse_path + "\r\n";
	if(this.hello_name) {
		inserted_header_fields += format(
						"Received: from %s (%s [%s])\r\n" +
						"          by %s [%s] (%s)\r\n" +
						"          for %s; %s %s\r\n"
						,client.host_name,this.hello_name,client.ip_address
						,system.host_name,client.socket.local_ip_address
						,server.version
						,"unknown"
						,strftime("%a, %d %b %Y %H:%M:%S"),system.zonestr()
						);
	}
	log(LOG_DEBUG, "inserted headers = " + inserted_header_fields);
	content_length += inserted_header_fields.length;

	sock.write(command.toUpperCase()+" SPAMC/1.2\r\n");
	sock.write("Content-length: "+content_length+"\r\n");
	if(this.user)	// Optional
		sock.write("User: " + this.user + "\r\n");
	sock.write("\r\n");
	sock.write(inserted_header_fields);
	sock.sendfile(this.messagefile);
	sock.is_writeable=false;

	while(1) {
		tmp=sock.recvline();
		if(tmp==undefined || tmp=='')
			break;
		if(this.debug)
			log(LOG_DEBUG,"RX SPAMD header: " + tmp);
		rcvd.push(tmp);
	}
	if(sock.is_connected) {
		// Read the body
		while(1) {
			tmp=sock.recv();
			if(tmp==undefined || tmp=='')
				break;
			ret.message += tmp;
		}
	}

	if(rcvd.length < 1) {
		ret.error='No lines read from spamd';
		return(ret)
	}

	tmp=rcvd[0].split(/\s+/);	/* was ,3 */
	if(tmp.length < 3) {
		ret.error="Unable to parse line '"+rcvd[0];
		return(ret)
	}
	if(tmp[1] != '0') {
		tmp.shift();
		ret.error="spamd returned error: " + tmp.join(" ");
		return(ret)
	}

	/* Parse headers */
	for(line=1; line<rcvd.length; line++) {
		if(rcvd[line]=='')
			break;
		tmp=rcvd[line].split(/\s+/);
		switch(tmp[0].toUpperCase()) {
			case 'SPAM:':
				if(tmp[1].toLowerCase() == 'true' || tmp[1].toLowerCase() == 'yes')
					ret.isSpam=true;
				else
					ret.isSpam=false;
				if(tmp[2] == ';') {
					ret.score=parseFloat(tmp[3]);
					if(tmp[4] == '/')
						ret.threshold=parseFloat(tmp[5]);
				}
				break;
			case 'CONTENT-LENGTH:':
				if(!isNaN(tmp[1])) {
					var clen=parseInt(tmp[1]);
					if(clen < ret.message.length)
						ret.message = ret.message.substr(0, clen);
					if(clen > ret.message.length)
						ret.warning="Content-Length greater than total bytes read!";
				}
		}
	}

	if(command == 'SYMBOLS') {
		ret.message=ret.message.replace(/[\r\n]/g,'');
		ret.symbols=ret.message.split(/,/);
		ret.message='';
	}
	if(command == 'PROCESS') {
		var headers=ret.message.replace(/^([\x00-\xff]*?\r\n)\r\n[\x00-\xff]*$/,"$1");
		var m=headers.match(/X-Spam-Status:\s*([\x00-\xff]*?)\r\n[^\s]/);
		if(m!=null) {
			var hdr=m[1].replace(/,\r\n\s+/g,',');
			hdr=hdr.replace(/\s+/g,' ');
			var tokens=hdr.split(/\s+/);
			switch(tokens[0]) {
				case 'No,':
					ret.isSpam=false;
					break;
				case 'Yes,':
					ret.isSpam=true;
					break;
				default:
					ret.warning="Unknown initial X-Spam-Status token: '"+tokens[0]+"'";
					break;
			}
			for(var i=1; i<tokens.length; i++) {
				var nv=tokens[i].split(/=/);
				switch(nv[0]) {
					case 'score':
						if(!isNaN(nv[1]))
							ret.score=parseFloat(nv[1]);
						break;
					case 'required':
						if(!isNaN(nv[1]))
							ret.threshold=parseFloat(nv[1]);
						break;
					case 'tests':
						ret.symbols.push(nv[1].split(/,/));
						break;
				}
			}
		}
	}

	return(ret);
}
