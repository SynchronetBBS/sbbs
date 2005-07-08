load("sbbsdefs.js");
load("sockdefs.js");

var SUCCESS=0;
var SENDING_PR=1;
var QUERY_SUCCESS=2;
var ERROR=3;
var RETRY=4;

var s = new Socket(SOCK_STREAM);
var r;	/* Response object */
s.connect("gnats.bbsdev.net",1529);
if(!s.is_connected) {
	writeln("Cannot connect to GNATS database!");
	console.pause();
	exit();
}

r=get_response(s);
if(r.code!=200) {
	writeln("CONNECT Expected 200, got "+r.code);
	writeln(r.message);
	console.pause();
	exit();
}
s.send("USER guest\r\n");
r=get_response(s);
if(r.code!=210) {
	writeln("USER Expected 210, got "+r.code);
	writeln(r.message);
	console.pause();
	exit();
}
set_prlist(s);
s.send('QUER\r\n');
r=get_response(s);
prs=r.text.split(/\r\n/);
// Remove final blank entry
prs.pop();
if(r.code == 300) {
	var done=false;
	while(!done) {
		var c;
		for(c=0; c < prs.length; c++) {
			console.uselect(c,"Problem Report",prs[c],"");
		}
		pr=console.uselect();
		if(pr>=0) {
			s.send("QFMT full\r\n");
			r=get_response(s);
			if(r.code!=210) {
				writeln("QFMT Expected 210, got "+r.code);
				writeln(r.message);
				console.pause();
				exit();
			}
			m=prs[pr].match(/^[^\/]*\/([0-9]*)/);
			if(m!=undefined && m.index >-1) {
				s.send("QUER "+m[1]+"\r\n");
				r=get_response(s);
				if(r.code != 300) {
					writeln("QUER Expected 300, got "+r.code);
					writeln(r.message);
					console.pause();
					exit();
				}
				writeln(r.text);
				writeln();
				writeln("--- End of PR ---");
			}
			else {
				writeln("Error getting PR info");
				console.pause();
				exit();
			}
		}
		else
			done=true;
		set_prlist(s);
	}
}
else if (r.code == 220) {
	writeln("No PRs!");
	console.pause();
}
else {
	writeln("QUER Expected 300 or 220, got "+r.code);
	writeln(r.message);
	console.pause();
	exit();
}

s.send("QUIT\r\n");

function get_response(s)
{
	var resp = new Object;
	var done=false;
	var line;

	resp.message='';
	resp.text='';

	while(!done) {
		if(s.poll(30)) {
			resp.raw=s.recvline();
			var m=resp.raw.match(/^([0-9]{3})([- ])(.*)$/);
			if(m != undefined && m.index>-1) {
				resp.code=parseInt(m[1]);
				resp.message+=m[3];
				if(m[2]=='-')
					resp.message+="\r\n";
				else
					done=true;
				switch((resp.code/100).toFixed(0)) {
					case '2':
						resp.type=SUCCESS;
						break;
					case '3':
						if(resp.code < 350)
							resp.type=SENDING_PR;
						else
							resp.type=QUERY_SUCCESS;
						break;
					case '4':
					case '5':
						resp.type=ERROR;
						break;
					case '6':
						resp.type=RETRY;
						break;
				}
			}
		}
		else {
			writeln("Error while recieving response!");
			exit();
		}
	}
	if(resp.type==SENDING_PR) {
		done=false;
		while(!done) {
			if(s.poll(30)) {
				line=s.recvline();
				if(line != undefined) {
					if(line==".") {
						done=true;
					}
					else {
						line=line.replace(/^\.\./,'.');
						resp.text+=line+"\r\n";
					}
				}
				else {
					writeln("Error while recieving PR!");
					exit();
				}
			}
		}
	}
	return(resp);
}

function set_prlist(s)
{
	s.send('EXPR State != "Closed"\r\n');
	r=get_response(s);
	if(r.code!=210) {
		writeln("EXPR Expected 210, got "+r.code);
		console.pause();
		exit();
	}
	s.send('QFMT "%9.9s/%-5.5s %-45.45s %-10.10s" Category Number Synopsis Responsible State\r\n');
	r=get_response(s);
	if(r.code!=210) {
		writeln("QFMT Expected 210, got "+r.code);
		writeln(r.message);
		console.pause();
		exit();
	}
}
