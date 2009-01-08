/*
 * http://spamassassin.apache.org/full/3.0.x/dist/spamd/PROTOCOL
 * $ Id: $
 */

load("sockdefs.js")

var spamd_addr='127.0.0.1';	// TODO: .ini file
var spamd_port=783;		// TODO: .ini file

function SPAMC_Message(message)
{
	this.message=message;
	this.DoCommand=Message_DoCommand;
	this.check getter=function() { return(this.DoCommand('CHECK')); };
	this.symbols getter=function() { return(this.DoCommand('SYMBOLS')); };
	this.report getter=function() { return(this.DoCommand('REPORT')); };
	this.report_ifspam getter=function() { return(this.DoCommand('REPORT_IFSPAM')); };
	this.process getter=function() { return(this.DoCommand('PROCESS')); }; }
}

function Message_DoCommand(command)
{
	var rcvd=new Array();
	var tmp;
	var sock=new Socket(SOCK_STREAM, "spamc");
	var ret=new Object();

	command=command.toUpperCase();
	if(!sock.connect(spamd_addr, spamd_port)) {
		log("ERROR: spamc.js failed to connect!");
		return(false);
	}
	sock.write(command.toUpperCase()+" SPAMC/1.2\r\n");
	sock.write("Content-length: "+this.message.length+"\r\n");
	sock.write("User: Synchronet\r\n");
	sock.write("\r\n");
	sock.write(this.message);
	sock.is_writeable=false;
	while((tmp=sock.recvline())!=undefined) {
		rcvd.push(tmp);
	}
	if(rcvd.length < 1) {
		log("ERROR: No lines read from spamd");
		return(false)
	}
	var tmp=rcvd[0].split(/\s+/,3);
	if(tmp.length < 3) {
		log("ERROR: Unable to parse line '"+rcvd[0]);
		return(false)
	}
	if(tmp[1] != 0) {
		log("ERROR: spamd returned error "+tmp[2]+" ("+tmp[1]+")");
		return(false)
	}

	ret.message='';
	/* Parse headers */
	for(line=1; line<rcvd.length; line++) {
		if(rcvd[line]=='')
			break;
		tmp=rcvd[line].split(/\s+/);
		if(tmp[0].toUpperCase() == 'SPAM:') {
			if(tmp[1].toLowerCase() == 'true')
				ret.isSpam=true;
			else
				ret.isSpam=false;
			if(tmp[2] == ';') {
				ret.score=parseInt(tmp[3]);
				if(tmp[4] == '/')
					ret.threshold=parseInt(tmp[5]);
			}
		}
	}

	/* Parse message */
	for(line++; line<rcvd.length; line++)
		ret.message += rcvd[line]+"\r\n";

	if(command == 'SYMBOLS') {
		if(ret!==false) {
			ret.symbols=ret.message.split(/,/);
			ret.message='';
		}
	}

	return(ret);
}
