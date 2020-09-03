var lastmisc;
var laststatus;
try {
	lastmisc=system.node_list[bbs.node_num-1].misc;
	laststatus=system.node_list[bbs.node_num-1].status;
}
catch (er) {}

function CheckNode()
{
	/* Node status check */
	var newmisc=system.node_list[bbs.node_num-1].misc;
	var newstatus=system.node_list[bbs.node_num-1].status;
	if(newmisc != lastmisc || newstatus != laststatus) {
		console.saveline();
		bbs.nodesync();
		console.write("\r");
		if(console.line_counter!=0) {
			console.crlf();
			console.line_counter=0;
		}
		console.restoreline();
		lastmisc=system.node_list[bbs.node_num-1].misc;
		laststatus=system.node_list[bbs.node_num-1].status;
	}
}

function CheckTime()
{
	/* Time Check */
	if((player.TimedUsed + (time()-on_at)) > (Settings.MaxTime*60)) {
		console.crlf();
		console.crlf();
		console.writeln("You are out of time for today");
		exit(0);
	}
}

function CheckTerminate()
{
	if(js.terminated)
		exit(0);
}

function InputFunc(values)
{
	var str='';
	var pos=0;
	var insertmode=true;
	var key;
	var origattr=console.attributes;
	var matchval='';

	console.line_counter=0;
	console.attributes="N";
InputFuncMainLoop:
	for(;;) {
		CheckNode();
		CheckTime();
		CheckTerminate();

		key=console.inkey(K_NONE, 100);
		if(key == '') {
			/* Busy loop checking */
		}
		else {
			switch(key) {
				case '\x1b':	/* Escape */
					matchval='';
					pos=0;
					break InputFuncMainLoop;
				case '\r':
					break InputFuncMainLoop;
				case '\x08':	/* Backspace */
					if(pos==0)
						break;
					console.write('\x08 \x08');
					str=str.substr(0,-1);
					pos--;
					/* Fall-through */
				default:
					if(key != '\x08') {
						/* No CTRL chars (evar!) */
						if(ascii(key)<32)
							break;
						str=str.substr(0,pos)+key+str.substr(pos);
						pos++;
						console.write(key);
					}
					/* Is this an exact match AND the longest possible match? */
					var value;
					var exact_match=false;
					var longer_match=false;
					matchval='';
					for(value in values) {
						if(typeof(values[value])=='string') {
							var ucv=values[value].toUpperCase();
							var ucs=str.toUpperCase();
							if(ucv==ucs) {
								exact_match=true;
								matchval=values[value];
							}
							else if(ucv.indexOf(ucs)==0)
								longer_match=true;
						}
						else if(typeof(values[value])=='object') {
							var min=0;
							var max=4294967295;
							var def=0;
							if(values[value].min != undefined)
								min=values[value].min;
							if(values[value].max != undefined)
								max=values[value].max;
							if(values[value].def != undefined)
								def=values[value].def;
							if(str.search(/^[0-9]*$/)!=-1) {
								var cur=def;
								if(pos > 0)
									cur=parseInt(str);
								if(cur >= min && cur <= max) {
									exact_match=true;
									matchval=cur;
								}
								if(cur*10 <= max)
									longer_match=true;
							}
						}
					}
					if(exact_match && !longer_match)
						break InputFuncMainLoop;
					/* That's not valid! */
					if((!longer_match) && pos > 0) {
						console.write('\x08 \x08');
						str=str.substr(0,-1);
						pos--;
					}
			}
		}
	}

	/* Look for a default value */
	if(pos==0) {
		for(value in values) {
			if(typeof(values[value])=='object' && values[value].def != undefined) {
				matchval=values[value].def;
			}
		}
	}
	while(pos > 0) {
		console.write('\x08');
		pos--;
	}
	console.cleartoeol(origattr);
	console.write(matchval.toString());
	console.crlf();
	return(matchval);
}
