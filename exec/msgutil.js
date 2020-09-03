// $Id: msgutil.js,v 1.2 2020/04/25 00:49:32 rswindell Exp $

"use strict";

load('sbbsdefs.js');
load('822header.js');

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

function show_headers_fast(msgbase, include_votes)
{
	var hdrs = msgbase.get_all_msg_headers(include_votes);

	var i;
	for(i in hdrs) {
		var h;
		print(i);
		for(h in hdrs[i])
			print(format("%25s = ", h) + hdrs[i][h]);
	}
}

function show_headers(msgbase, option)
{
	var i;
	var total_msgs = msgbase.total_msgs;
	var errors = 0;
	for(i = 0; i < total_msgs; i++) {
		var hdr = msgbase.get_msg_header(true, i+1);
		print("[" + i + "]");
		if(!hdr) {
			alert(msgbase.status);
			errors++;
		} else if(option.json)
			print(lfexpand(JSON.stringify(hdr, null, 4)));
		else {
			for(var h in hdr)
				print(format("%25s = ", h) + hdr[h]);
		}
	}
	return errors;
}

function export_msgs(msgbase, option)
{
	var errors = 0;
	var hdrs = msgbase.get_all_msg_headers(option.votes);
	var i;
	for(i in hdrs) {
		var msg = hdrs[i];
		var fname = system.temp_dir + basecode + "_" + msg.number + ".txt";
		var f = new File(fname);
		if(!f.open("wb")) {
			alert("Error " + f.error + " opening " + f.name);
			errors++;
			continue;
		}
		var text = msgbase.get_msg_body(msg
					,/* strip ctrl-a */true
					,/* dot-stuffing */false
					,/* tails */true
					,/* plain-text */true);	
		if(!text) {
			errors++;
			continue;
		}
		f.write(msg.get_rfc822_header(/* force_update: */false, /* unfold: */false));
		f.write(text);
		f.close();
		print("Created " + f.name);
	}
	return errors;
}

var i;
var basecode;
var option = [];

for(i in argv) {
	if(argv[i].charAt(0) != '-') {
		if(!basecode)
			basecode = argv[i];
	} else
		option[argv[i].slice(1)] = true;
}

if(basecode == undefined) {
	alert("Message base not specified");
	exit();
}

var msgbase = MsgBase(basecode);
if(!msgbase.open()) {
	alert("Error " + msgbase.last_error + " opening " + basecode);
	exit(1);
}
var errors = 0;
for(i in option) {
	switch(i) {
		case "hdrs":
			errors += show_headers(msgbase, option);
			break;
		case "index":
			errors += show_index(msgbase, msgbase.first_msg, msgbase.last_msg, option.votes);
			break;
		case "export":
			errors += export_msgs(msgbase, option);
			break;
	}
}
print(errors + " errors");
