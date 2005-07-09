load("sockdefs.js");
load("mailutil.js");
load("sbbsdefs.js");

var PR_SUCCESS=0;
var PR_SENDING_DATA=1;
var PR_QUERY_SUCCESS=2;
var PR_ERROR=3;
var PR_RETRY=4;

function GNATS(host,user,pass,email)
{
	// Properties
	if(host == undefined)
		this.host='gnats.bbsdev.net';
	else
		this.host=host;
	if(user == undefined)
		this.user=guest;
	else
		this.user=user;
	if(pass == undefined)
		this.pass=''
	else
		this.pass=pass;
	if(email==undefined)
		email='bugs@'+this.host;
	else
		this.email=email;
	this.error='';
	this.response=new Object;
	this.response.message='';
	this.response.text='';
	this.response.raw='';
	this.response.code=0;
	this.response.type=PR_ERROR;
	this.socket=new Socket(SOCK_STREAM);

	// Methods
	this.connect=GNATS_connect;
	this.close=GNATS_close;
	this.get_fullpr=GNATS_get_fullpr;
	this.expect=GNATS_expect;
	this.cmd=GNATS_cmd;
	this.get_response=GNATS_get_response;
	this.set_qfmt=GNATS_set_qfmt;
	this.set_expr=GNATS_set_expr;
	this.reset_expr=GNATS_reset_expr;
	this.get_result=GNATS_get_result;
	this.get_results=GNATS_get_results;
	this.get_list=GNATS_get_list;
	this.get_valid=GNATS_get_valid;
	this.submit=GNATS_submit;
	this.and_expr=GNATS_and_expr;
	this.append=GNATS_append;
	this.replace=GNATS_replace;
	this.get_field=GNATS_get_field;
	this.send_followup=GNATS_send_followup;
}

function GNATS_connect()
{
	this.socket.connect(this.host,1529);
	if(!this.socket.is_connected) {
		this.error="Cannot connect to GNATS database";
		return(false);
	}
	if(!this.get_response())
		return(false);
	if(!this.expect("CONNECT",200))
		return(false);
	if(!this.cmd("USER",this.user,this.pass))
		return(false);
	if(!this.expect("USER",210))
		return(false);
	return(true);
}

function GNATS_close()
{
	if(!this.socket.is_connected) {
		this.error="Socket not connected";
		return(false);
	}

	this.cmd("QUIT");
	this.socket.close();
	return(true);
}

function GNATS_get_fullpr(num)
{
	if(!this.reset_expr())		// Reset current expression.
		return(undefined);
	if(!this.set_qfmt("full"))		// Request full PR
		return(undefined);
	if(!this.cmd("QUER",num))		// Get PR
		return(undefined);
	if(!this.expect("QUER",300))
		return(undefined);
	return(this.response.text);
}

function GNATS_expect()
{
	var i;
	var args=new Array();
	var desc;

	desc=arguments[0];
	for(i=1; i<arguments.length; i++)
		args.push(arguments[i]);
	for(i=0; i<args.length; i++) {
		if(this.response.code == args[i])
			return(true);
	}
	this.error=desc+" expected "+args.join(" or ")+" got "+this.response.code+"\r\n"+this.response.message;

	return(false);
}

function GNATS_cmd()
{
	var send;
	var i;
	var args=new Array();

	for(i=0; i<arguments.length; i++)
		args.push(arguments[i]);

	send=args.join(' ')+"\r\n";
	if(!this.socket.is_connected) {
		this.error="Socket not connected";
		return(false);
	}
	if(!this.socket.send(send)) {
		this.error="Error sending on socket";
		return(false);
	}
	return(this.get_response());
}

function GNATS_get_response()
{
	var line;
	var m;
	var done=false;

	this.error='';
	this.response.message='';
	this.response.text='';
	this.response.raw='';
	this.response.code=0;
	this.response.type=PR_ERROR;

	if(!this.socket.is_connected) {
		this.error="Socket not connected";
		return(false);
	}

	while(!done) {
		if(this.socket.poll(30)) {
			line=this.socket.recvline();
			this.response.raw += line;
			m=line.match(/^([0-9]{3})([- ])(.*)$/);
			if(m != undefined && m.index>-1) {
				this.response.code=parseInt(m[1]);
				this.response.message+=m[3];
				if(m[2]=='-')
					this.response.message+="\r\n";
				else
					done=true;
				switch((this.response.code/100).toFixed(0)) {
					case '2':
						this.response.type=PR_SUCCESS;
						break;
					case '3':
						if(this.response.code < 350)
							this.response.type=PR_SENDING_DATA;
						else
							this.response.type=PR_QUERY_SUCCESS;
						break;
					case '4':
					case '5':
						this.response.type=PR_ERROR;
						this.error+=this.response.message;
						break;
					case '6':
						this.response.type=PR_RETRY;
						this.error+=this.response.message;
						break;
				}
			}
			else {
				this.error="Malformed response";
				return(false);
			}
		}
		else {
			this.error="Timeout waiting for response";
			return(false);
		}
	}
	if(this.response.type==PR_SENDING_DATA) {
		done=false;
		while(!done) {
			if(this.socket.poll(30)) {
				line=this.socket.recvline();
				if(line != undefined) {
					if(line==".") {
						done=true;
					}
					else {
						line=line.replace(/^\.\./,'.');
						this.response.text+=line+"\r\n";
					}
				}
				else {
					this.error="Error while recieving data";
					return(false);
				}
			}
			else {
				this.error="Timeout waiting for data";
				return(false);
			}
		}
	}
	return(true);
}

function GNATS_set_qfmt(format)
{
	if(format==undefined)
		format="standard";
	if(!this.cmd("QFMT",format))
		return(false);
	if(!this.expect("QFMT",210))
		return(false);
	return(true);
}

function GNATS_set_expr()
{
	var i;
	var args=new Array();

	for(i=0; i<arguments.length; i++)
		args.push(arguments[i]);

	if(!this.reset_expr())
		return(false);
	if(!this.cmd("EXPR",args.join(" & ")))
		return(false);
	if(!this.expect("EXPR",210))
		return(false);
	return(true);
}

function GNATS_reset_expr()
{
	if(!this.cmd("RSET"))
		return(false);
	if(!this.expect("RSET",210))
		return(false);
	return(true);
}

function GNATS_get_result(num)
{
	if(!this.cmd("QUER",num))		// Get PR
		return(undefined);
	if(!this.expect("QUER",300))
		return(undefined);
	return(this.response.text);
}

function GNATS_get_results(format)
{
	var i;
	var prs;
	var result;
	var results = new Array();

	if(format==undefined)
		format="standard";
	if(!this.set_qfmt('"%s" Number'))
		return(undefined);
	if(!this.cmd("QUER"))
		return(undefined);
	if(!this.expect("QUER",300,220))
		return(undefined);
	prs=this.response.text.split(/\r?\n/);
	prs.pop()	// Remove the empty field at end
	if(!this.set_qfmt(format))
		return(undefined);
	for(i in prs) {
		result=this.get_result(prs[i]);
		if(result != undefined) {
			results.push(result);
		}
	}
	return(results);
}

function GNATS_get_list(type)
{
	var items;

	if(!this.cmd("LIST",type))
		return(undefined);
	if(!this.expect("LIST",301))
		return(undefined);
	items=this.response.text.split(/\r?\n/);
	items.pop()	// Remove empty field at end
	return(items);
}

function GNATS_get_valid(field)
{
	var items;

	if(!this.cmd("FVLD",field))
		return(undefined);
	if(!this.expect("FVLD",301))
		return(undefined);
	items=this.response.text.split(/\r?\n/);
	items.pop()	// Remove empty field at end
	return(items);
}

function GNATS_submit(pr)
{
	var lines;
	var i;

	if(!this.cmd("SUBM"))
		return(false);
	if(!this.expect("SUBM",211))
		return(false);
	lines=pr.split(/\r?\n/);
	for(i=0; i<lines.length; i++) {
		if(!this.socket.send(lines[i].replace(/^\./,'..')+"\r\n")) {
			this.error="Error sending PR";
			this.close();
			return(false);
		}
	}
	if(!this.socket.send(".\r\n")) {
		this.error="Error sending PR";
		this.close();
		return(true);
	}
	if(!this.get_response())
		return(false);
	if(!this.expect("SUBM",200))
		return(false);
	return(true);
}

function GNATS_and_expr(expr)
{
	var i;
	var args=new Array();

	for(i=0; i<arguments.length; i++)
		args.push(arguments[i]);

	if(!this.cmd("EXPR",args.join(" & ")))
		return(false);
	if(!this.expect("EXPR",210))
		return(false);
	return(true);
}

function GNATS_get_field(pr,field)
{
	var retval;

	if(!this.set_qfmt('"%s" '+field))
		return(false);
	return(this.get_result(pr));
}

function GNATS_append(pr,field,newval,reason)
{
	if(!this.cmd("APPN",pr,field))
		return(false);
	if(!this.expect("APPN",212))
		return(false);
	lines=newval.split(/\r?\n/);
	for(i=0; i<lines.length; i++) {
		if(!this.socket.send(lines[i].replace(/^\./,'..')+"\r\n")) {
			this.error="Error sending data";
			this.close();
			return(false);
		}
	}
	if(!this.socket.send(".\r\n")) {
		this.error="Error sending data";
		this.close();
		return(true);
	}
	if(!this.get_response())
		return(false);
	if(!this.expect("APPN",210,213))
		return(false);
	if(this.response.code==213) {	// Need to send a reason
		lines=reason.split(/\r?\n/);
		for(i=0; i<lines.length; i++) {
			if(!this.socket.send(lines[i].replace(/^\./,'..')+"\r\n")) {
				this.error="Error sending data";
				this.close();
				return(false);
			}
		}
		if(!this.socket.send(".\r\n")) {
			this.error="Error sending data";
			this.close();
			return(true);
		}
		if(!expect("APPN",210))
			return(false);
	}
	return(true);
}

function GNATS_replace(pr,field,newval,reason)
{
	if(!this.cmd("REPL",pr,field))
		return(false);
	if(!this.expect("REPL",212))
		return(false);
	lines=newval.split(/\r?\n/);
	for(i=0; i<lines.length; i++) {
		if(!this.socket.send(lines[i].replace(/^\./,'..')+"\r\n")) {
			this.error="Error sending data";
			this.close();
			return(false);
		}
	}
	if(!this.socket.send(".\r\n")) {
		this.error="Error sending data";
		this.close();
		return(true);
	}
	if(!this.get_response())
		return(false);
	if(!this.expect("REPL",210,213))
		return(false);
	if(this.response.code==213) {	// Need to send a reason
		lines=reason.split(/\r?\n/);
		for(i=0; i<lines.length; i++) {
			if(!this.socket.send(lines[i].replace(/^\./,'..')+"\r\n")) {
				this.error="Error sending data";
				this.close();
				return(false);
			}
		}
		if(!this.socket.send(".\r\n")) {
			this.error="Error sending data";
			this.close();
			return(true);
		}
		if(!expect("REPL",210))
			return(false);
	}
	return(true);
}

function GNATS_send_followup(pr,name,from,message)
{
	var hdrs = new Object;
	var recips = new Array();

	hdrs.to='bugs';
	hdrs.to_net_type=NET_INTERNET;
	hdrs.to_net_addr=this.email;

	hdrs.rcpt_list = new Array();
	var orig=this.get_field(pr,'Reply-To');
	var recip = new Object;
	recip.to=mail_get_name(orig);
	recip.to_net_type=NET_INTERNET;
	recip.to_net_addr=mail_get_address(orig);
	hdrs.rcpt_list.push(recip);

	hdrs.from=name;
	hdrs.from_net_type=NET_INTERNET;
	hdrs.from_net_addr=from;
	hdrs.replyto=name;
	hdrs.replyto_net_type=NET_INTERNET;
	hdrs.replyto_net_addr=from;

	if(!this.set_qfmt('"Re: %s/%d: %s" Category Number Synopsis'))
		return(false);
	var subject=this.get_result(pr);
	if(subject==undefined)
		return(false);
	hdrs.subject=subject.replace(/[\r\n]/g,'');

	var msgbase = new MsgBase('mail');
	if(msgbase.open!=undefined && msgbase.open()==false) {
		this.error='Cannot open message base';
		return(false);
	}
	if(!msgbase.save_msg(hdrs, message)) {
		this.error='Error saving message: '+msgbase.error;
		msgbase.close();
		return(false);
	}
	msgbase.close();
	return(true);
}
