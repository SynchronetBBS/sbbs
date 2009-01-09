/*
 * http://spamassassin.apache.org/full/3.0.x/dist/spamd/PROTOCOL
 * $Id$
 */

load("sockdefs.js")

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

	if(!sock.connect(this.addr, this.port)) {
		ret.error='Failed to connect to spamd';
		return(ret);
	}

	sock.write(command.toUpperCase()+" SPAMC/1.2\r\n");
	sock.write("Content-length: "+file_size(this.messagefile)+"\r\n");
	if(this.user)	// Optional
		sock.write("User: " + this.user + "\r\n");
	sock.write("\r\n");
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

	tmp=rcvd[0].split(/\s+/,3);
	if(tmp.length < 3) {
		ret.error="Unable to parse line '"+rcvd[0];
		return(ret)
	}
	if(tmp[1] != '0') {
		ret.error="spamd returned error "+tmp[2]+" ("+tmp[1]+")";
		return(ret)
	}

	/* Parse headers */
	for(line=1; line<rcvd.length; line++) {
		if(rcvd[line]=='')
			break;
		tmp=rcvd[line].split(/\s+/);
		switch(tmp[0].toUpperCase()) {
			case 'SPAM:':
				if(tmp[1].toLowerCase() == 'true')
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

	return(ret);
}
