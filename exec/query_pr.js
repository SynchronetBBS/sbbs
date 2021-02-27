// $Id: query_pr.js,v 1.20 2005/08/05 21:26:02 deuce Exp $

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
query.text.desc="Single Line Text Fields";
query.multitext=new Object;
query.multitext.text="Any";
query.multitext.expr=undefined;
query.multitext.field="fieldtype:Multitext";
query.multitext.list=undefined;
query.multitext.desc="Multi Line Text Fields";
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

var gnats_user="guest";
var password=undefined;
if(argc>0)
	gnats_user=argv[0];
if(argc>1)
	password=argv[1];
var gnats = new GNATS("bugs.synchro.net",gnats_user,password);

if(!gnats.connect())
	handle_error();

var done=false;
while(!done && bbs.online) {
	done=set_prlist();
	if(done)
		break;
	prs=gnats.get_results('"%-72.72s %-10.10s %-6.6s %-10.10s %-10.10s" Synopsis Category Number Responsible State');
	if(prs==undefined)
		handle_error();
	if(prs.length > 0) {
		var donelist=false;
		var c;
		while(!donelist && bbs.online) {
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
					if(gnats.access >= GNATS_LEVEL_EDIT && 
							!console.noyes("Modify/Remove this PR")) {
						var fields=gnats.get_list("FieldNames");
						for(c=0; c<fields.length; c++) {
							console.uselect(c,"Field",fields[c],"");
						}
						console.uselect(fields.length,"Fields","Delete this PR","");
						var field=console.uselect();
						if(field>=0 && field<fields.length) {
							var oldval=gnats.get_field(m[1],fields[field]);
							var newval=undefined;
							if(oldval==undefined)
								oldval='';
							if(!gnats.cmd("FTYP",fields[field]))
								handle_error();
							if(!gnats.expect("FTYP",350))
								handle_error();
							switch(gnats.response.message) {
								case 'Text':
								case 'TextWithRegex':
								case 'Date':
									oldval=oldval.replace(/[\r\n]/g,'');
									var newval=console.getstr(oldval,78,K_EDIT);
									if(console.aborted)
										newval=undefined;
									break;
								case 'MultiText':
									writeln("Cannot yet modify multitext fields, sorry.");
									break;
								case 'Enum':
									var vals=gnats.get_valid(fields[field]);
									if(vals==undefined)
										handle_error();
									for(c=0; c<vals.length; c++) {
										console.uselect(c, "New Value", vals[c], "");
									}
									c=console.uselect();
									if(c>=0 && c<vals.length)
										newval=vals[c];
									break;
								case 'MultiEnum':
									var vals=gnats.get_valid(fields[field]);
									if(vals==undefined)
										handle_error();
									oldval=oldval.replace(/[\r\n]/g,'');
									var sep=',';
									if(oldval.search(/:/)>-1)
										sep=':';
									var cvals=oldval.split(/:,/);
									var cv = new Object;
									for(c=0; c<cvals.length; c++)
										cv[cvals[c]]=true;
									var doneenum=false;
									while(!doneenum && bbs.online) {
										for(c=0; c<vals.length; c++) {
											if(cv[vals[c]] == undefined || cv[vals[c]]==false)
												console.uselect(c, "New Values", vals[c], "");
											else 
												console.uselect(c, "New Values", vals[c]+ '(Selected)', "");
										}
										console.uselect(c, "New Values", "Save Changes", "");
										c=console.uselect();
										if(c<0)
											break;
										else if(c>=0 && c<vals.length) {
											if(cv[vals[c]] == undefined || cv[vals[c]]==false)
												cv[vals[c]]=true;
											else
												cv[vals[c]]=false;
										}
										else if(c==vals.length)
											doneenum=true;
									}
									if(doneenum) {
										var newvals=new Array();
										for(c=0; c<vals.length; c++) {
											if(cv[vals[c]] != undefined && cv[vals[c]]==true)
												newvals.push(vals[c]);
										}
										newval=newvals.join(sep);
									}
									break;
								case 'Integer':
									oldval=oldval.replace(/[\r\n]/g,'');
									var newval=console.getstr(oldval,78,K_EDIT|K_NUMBER);
									if(console.aborted)
										newval=undefined;
									break;
							}
							if(!bbs.online)
								newval=undefined;
							if(newval != undefined) {
								var reason='';
								if(!gnats.cmd("FIELDFLAGS",fields[field]))
									handle_error();
								if(!gnats.expect("FIELDFLAGS",350))
									handle_error();
								if(gnats.response.message.search(/\brequireChangeReason\b/)>-1) {
									console.print("\1y\1hFollowup message (Blank line ends):\r\n");
									do {
										var line=console.getstr();
										if(console.aborted)
											break;
										reason += line + "\r\n";
									} while (line != '' && bbs.online);
								}
								if(!gnats.replace(m[1],fields[field],newval,reason))
									handle_error();
							}
						}
						else if(field==fields.length) {
							if(console.yesno("Are you sure you wish to delete this PR?")) {
								if(!gnats.cmd("DELETE",m[1]))
									handle_error();
								if(!gnats.expect("DELETE",210))
									handle_error();
								continue;
							}
						}
					}
					var pr=gnats.get_fullpr(m[1]);
					if(pr==undefined)
						handle_error();
					writeln(pr);
					writeln();
					writeln("--- End of PR ---");
					if(!console.noyes("Submit a followup")) {
						console.print("\1y\1hFollowup message (Blank line ends):\r\n");
						var note = '';
						do {
							var line=console.getstr();
							if(console.aborted)
								break;
							note += line + "\r\n";
						} while (line != '' && bbs.online);
						if(line == '' && bbs.online) {
							if(!gnats.send_followup(m[1],user.name,user.email,note))
								handle_error();
						}
					}
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
	while(!done && bbs.online) {
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
						expr += ' "'+cols[0].replace(/"/g,'\\"')+'"';
						query[fields[f]].text=text;
						query[fields[f]].expr=expr;
					}
				}
				else {
					write(query[fields[f]].desc+" "+text+": ");
					var val=console.getstr();
					text += ' "'+val+'"';
					val.replace(/"/g,'\\"');
					expr += ' "'+val.replace(/"/g,'\\"')+'"';
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
	clean_exit(1);
}
