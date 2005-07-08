load("sbbsdefs.js");
load("sockdefs.js");

var SUCCESS=0;
var SENDING_PR=1;
var QUERY_SUCCESS=2;
var ERROR=3;
var RETRY=4;

var query=new Object;

// Set up query object... this probobly SHOULD be done with server data.
query.text=new Object;
query.text.text="Any";
query.text.expr=undefined;
query.text.field="fieldtype:Text";
query.text.list=undefined;
query.text.desc="Text Fields";
query.category=new Object;
query.category.text="Any";
query.category.expr=undefined;
query.category.field="Category";
query.category.list="Categories";
query.category.listdesc=1;
query.category.desc="Category";
query.number=new Object;
query.number.text="Any";
query.number.expr=undefined;
query.number.field="Number";
query.number.list=undefined;
query.number.desc="PR Number";
query.responsible=new Object;
query.responsible.text="Any";
query.responsible.expr=undefined;
query.responsible.field="Responsible";
query.responsible.list="Responsible";
query.responsible.listdesc=1;
query.responsible.desc="Responsible";
query.state=new Object;
query.state.text='Doesn'+"'"+'t equal "closed"';
query.state.expr='State != "closed"';
query.state.field="State";
query.state.list="States";
query.state.listdesc=2;
query.state.desc="State";

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
	clean_exit(s);
}
s.send("USER guest\r\n");
r=get_response(s);
if(r.code!=210) {
	writeln("USER Expected 210, got "+r.code);
	writeln(r.message);
	console.pause();
	clean_exit(s);
}
var done=false;
while(!done) {
	done=set_prlist(s);
	if(done)
		break;
	s.send('QUER\r\n');
	r=get_response(s);
	prs=r.text.split(/\r\n/);
	// Remove final blank entry
	prs.pop();
	if(r.code == 300) {
		var donelist=false;
		var c;
		while(!donelist) {
			for(c=0; c < prs.length; c++) {
				m=prs[c].match(/^(.{72}) (.{10}) (.{6}) (.{10}) (.{10})$/);
				if(m!=undefined && m.index>-1) {
					console.uselect(c,"Problem Report",format("%s\r\n     State: %s Responsible: %s Category: %s PR: %s",m[1],m[5],m[4],m[2],m[3]),"");
				}
			}
			pr=console.uselect();
			if(pr>=0) {
				s.send("QFMT full\r\n");
				r=get_response(s);
				if(r.code!=210) {
					writeln("QFMT Expected 210, got "+r.code);
					writeln(r.message);
					console.pause();
				}
				m=prs[pr].match(/.{84}([0-9]*)/);
				if(m!=undefined && m.index >-1) {
					s.send("QUER "+m[1]+"\r\n");
					r=get_response(s);
					if(r.code != 300) {
						writeln("QUER Expected 300, got "+r.code);
						writeln(r.message);
						console.pause();
						clean_exit(s);
					}
					writeln(r.text);
					writeln();
					writeln("--- End of PR ---");
				}
				else {
					writeln("Error getting PR info");
					console.pause();
					clean_exit(s);
				}
			}
			else
				donelist=true;
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
		clean_exit(s);
	}
}
clean_exit(s);

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
			clean_exit(s);
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
					clean_exit(s);
				}
			}
		}
	}
	return(resp);
}

function set_prlist(s)
{
	var i;
	var j;
	var f;
	var done=false;
	fields=new Array();
	for (field in query) {
		fields.push(field);
	}
	while(!done) {
		var text='';
		var expr='';
		console.uselect(0,"Field","Run Query","");
		for(i=0;i<fields.length;i++) {
			console.uselect(i+1,"Field",query[fields[i]].desc+": "+query[fields[i]].text,"");
		}
		f=console.uselect();
		if(f==-1)
			return(true);
		if(f==0)
			break;
		f--;
		if(f>=0 && f < fields.length) {
			// Field selected to change...
			console.uselect(0,query[fields[f]].desc,"Equals","");
			console.uselect(1,query[fields[f]].desc,"Does not equal","");
			console.uselect(2,query[fields[f]].desc,"Contains","");
			console.uselect(3,query[fields[f]].desc,"Is greater than","");
			console.uselect(4,query[fields[f]].desc,"Is less than","");
			console.uselect(5,query[fields[f]].desc,"Any","");
			var op=console.uselect();
			switch(op) {
				case 0:
					text += "Equals";
					expr += query[fields[f]].field+"==";
					break;
				case 1:
					text += "Doesn't equal";
					expr += query[fields[f]].field+"!=";
					break;
				case 2:
					text += "Contains";
					expr += query[fields[f]].field+"~";
					break;
				case 3:
					text += "Is greater than";
					expr += query[fields[f]].field+">";
					break;
				case 4:
					text += "Is less than";
					expr += query[fields[f]].field+"<";
					break;
				case 5:
					text = "Any";
					expr=undefined;
					query[fields[f]].text=text;
					query[fields[f]].expr=expr;
					continue;
					break;
			}
			if(op>=0 && op <= 4) {
				if(query[fields[f]].list != undefined) {
					s.send("LIST "+query[fields[f]].list+"\r\n");
					r=get_response(s);
					if(r.code!=301) {
						writeln("LIST Expected 301, got "+r.code);
						writeln(r.message);
						console.pause();
						clean_exit(s);
					}
					vals=r.text.split(/\r\n/);
					vals.pop();	// Remove blank at end
					for(i=0; i<vals.length; i++) {
						cols=vals[i].split(/:/);
						console.uselect(i, query[fields[f]].desc, cols[0]+" ("+cols[query[fields[f]].listdesc]+")", "");
					}
					var val=console.uselect();
					if(val>0 && val < vals.length) {
						cols=vals[val].split(/:/);
						text += ' "'+cols[0]+'"';
						expr += ' "'+cols[0].replace(/"/,'\\"')+'"';
						query[fields[f]].text=text;
						query[fields[f]].expr=expr;
					}
				}
				else {
					write(query[fields[f]].desc+" "+text+": ");
					var val=console.getstr();
					text += ' "'+val+'"';
					val.replace(/"/,'\\"');
					expr += ' "'+val.replace(/"/,'\\"')+'"';
					query[fields[f]].text=text;
					query[fields[f]].expr=expr;
				}
			}
		}
	}
	var expr="State==State";	// Must have SOME query...
	for(field in fields) {
		if(query[fields[field]].expr != undefined) {
			expr += " & ";
			expr += query[fields[field]].expr;
		}
	}
	s.send('RSET\r\n');
	r=get_response(s);
	if(r.type!=SUCCESS) {
		writeln("RSET got unexpected "+r.code);
		writeln(r.message);
		console.pause();
		clean_exit(s);
	}
	s.send('EXPR '+expr+'\r\n');
	r=get_response(s);
	if(r.code!=210) {
		writeln("EXPR Expected 210, got "+r.code);
		writeln(r.message);
		console.pause();
		clean_exit(s);
	}
	s.send('QFMT "%-72.72s %-10.10s %-6.6s %-10.10s %-10.10s" Synopsis Category Number Responsible State\r\n');
	r=get_response(s);
	if(r.code!=210) {
		writeln("QFMT Expected 210, got "+r.code);
		writeln(r.message);
		console.pause();
		clean_exit(s);
	}
}

function clean_exit(s,code)
{
	s.send("QUIT\r\n");
	s.close();
	exit(code);
}
