// $Id$

load('sbbsdefs.js');

function show_index(msgbase, first_msg, last_msg, include_votes)
{
	var i;

	var total_msgs = msgbase.total_msgs;
	for(i = 0; i < total_msgs; i++) {
//		print(i);
		var idx = msgbase.get_msg_index(true, i, include_votes);
		if(!idx)
			continue;
		print(i);
		var p;
		for(p in idx)
			print(format("%10s = ", p) + idx[p]);
	}
}

function show_headers(msgbase, include_votes)
{
	alert("include votes = " + include_votes);
	var hdrs = msgbase.get_all_msg_headers(include_votes);

	var i;
	for(i in hdrs) {
		var h;
		print(i);
		for(h in hdrs[i])
			print(format("%25s = ", h) + hdrs[i][h]);
	}
}

var i;
var basecode;
var option = [];

for(i in argv)
	if(argv[i].charAt(0) != '-') {
		if(!basecode)
			basecode = argv[i];
	} else
		option[argv[i].slice(1)] = true;

if(basecode == undefined) {
	alert("Message base not specified");
	exit();
}

var start = time();
var msgbase = MsgBase(basecode);
if(!msgbase.open()) {
	alert("Error " + msgbase.last_error + " opening " + basecode);
	exit();
}
show_index(msgbase, msgbase.first_msg, msgbase.last_msg, option.votes);
show_headers(msgbase, option.votes);
print(Number(time() - start) + " seconds");
