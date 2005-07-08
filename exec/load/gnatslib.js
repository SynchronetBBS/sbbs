load("sockdefs.js");

var PR_SUCCESS=0;
var PR_SENDING_DATA=1;
var PR_QUERY_SUCCESS=2;
var PR_ERROR=3;
var PR_RETRY=4;

function GNATS(host,user,pass)
{
	// Properties
	this.host=host;
	this.user=user;
	this.pass=pass;
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
	if(!this.expect("USER",210);
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
	if(!this.set_expr("State==State"));	// Select all PRs
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
	for(i=1; i<arguments.length; i++) {
		if(this.response.code == arguments[i])
			return(true);
	}
	this.error=arguments.unshift()+" expected "+arguments.join(" or ")+"\r\n"+this.response.message;
	return(false);
}

function GNATS_cmd()
{
	var send=arguments.join(" ")+"\r\n";

	if(!this.socket.is_connected) {
		this.error="Socket not connected";
		return(false);
	}

	this.socket.send(send);
	return(this.get_response);
}

function GNATS_get_response()
{
	var line;
	var m;

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
			resp.raw+=this.socket.recvline();
			m=resp.raw.match(/^([0-9]{3})([- ])(.*)$/);
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
	if(!this.cmd("QFMT",format)
		return(false);
	if(!this.expect("QFMT",210))
		return(FALSE);
	return(true);
}

function GNATS_set_expr()
{
	if(!this.reset_expr())
		return(false);
	if(!this.cmd("EXPR",arguments.join(" "))
		return(false);
	if(!this.expect("EXPR",210))
		return(FALSE);
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
	var success=0;
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
			success++;
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





