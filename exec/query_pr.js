load("sbbsdefs.js");
load("sockdefs.js");
load("gnatslib.js");

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

var gnats = new GNATS("localhost","guest");

if(!gnats.connect())
	handle_error();

var done=false;
while(!done) {
	done=set_prlist();
	if(done)
		break;
	prs=gnats.get_results('"%-72.72s %-10.10s %-6.6s %-10.10s %-10.10s" Synopsis Category Number Responsible State');
	if(prs==undefined)
		handle_error();
	if(prs.length > 0) {
		var donelist=false;
		var c;
		while(!donelist) {
			for(c=0; c < prs.length; c++) {
				m=prs[c].match(/^(.{72}) (.{10}) (.{6}) (.{10}) (.{10})\r\n$/);
				if(m!=undefined && m.index>-1) {
					console.uselect(c,"Problem Report",format("%s\r\n     State: %s Responsible: %s Category: %s PR: %s",m[1],m[5],m[4],m[2],m[3]),"");
				}
			}
			pr=console.uselect();
			if(pr>=0) {
				m=prs[pr].match(/.{84}([0-9]*)/);
				if(m!=undefined && m.index >-1) {
					var pr=gnats.get_fullpr(m[1]);
					if(pr==undefined)
						handle_error();
					writeln(pr);
					writeln();
					writeln("--- End of PR ---");
				}
				else {
					writeln("Error getting PR info");
					console.pause();
					clean_exit();
				}
			}
			else
				donelist=true;
		}
	}
	else {
		writeln("No PRs!");
		console.pause();
	}
}
clean_exit();

function set_prlist()
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
					var vals=gnats.get_list(query[fields[f]].list);
					if(vals==undefined)
						handle_error();
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
	if(!gnats.reset_expr())
		handle_error();
	for(field in fields) {
		if(query[fields[field]].expr != undefined) {
			if(!gnats.and_expr(query[fields[field]].expr))
				handle_error();
		}
	}
}

function clean_exit(s,code)
{
	gnats.close();
	exit(code);
}

function handle_error()
{
	writeln(gnats.error);
	console.pause();
	clean_exit();
}
