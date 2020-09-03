// $Id: scrubmsgs.js,v 1.2 2018/02/16 02:29:24 rswindell Exp $

// Scrub msg headers of invalid control characters (sign of corruption) in one or more message bases

var debug = false;
var scan_only = false;

var hfields_to_scrub = [
	"subject",
	"id",
	"reply_id",
	"ftn_msgid",
	"ftn_reply",
	"to",
	"to_ext",
	"to_net_addr",
	"from",
	"from_ext",
	"from_org",
	"from_net_addr",
];

function scrub_base(msgbase)
{
	var count = 0;
	var total_msgs = msgbase.total_msgs;
	for(var n = 0; n < total_msgs && !js.terminated; n++) {
		var hdr = msgbase.get_msg_header(/* by_offset: */true, n, /* expand_fields: */false);
		if(!hdr) {
			if(debug)
				alert("Error " + msgbase.last_error + " reading message #" + (n+1));
			continue;
		}
		var copy = JSON.stringify(hdr);
//		print(copy);
		for(var f in hfields_to_scrub) {
			if(hdr[hfields_to_scrub[f]] != undefined)
				hdr[hfields_to_scrub[f]] = strip_ctrl(hdr[hfields_to_scrub[f]]);
		}
		if(JSON.stringify(hdr) != copy) {
			print("Corrupted message header #" + (n + 1));
			print(JSON.stringify(hdr, null, 4));
			if(scan_only)
				count++;
			else {
				if(!msgbase.put_msg_header(/* by_offset: */true, n, hdr)) {
					alert(msgbase.last_error);
				} else {
					count++;
				}
			}
		}
	}
	return count;
}

function open_and_scrub_base(code)
{
	var msgbase = new MsgBase(code);

	if(!msgbase.open()) {
		alert("Error opening " + code + " " + msgbase.last_error);
		return false;
	}
	print("Scanning " + msgbase.file);
	var result = scrub_base(msgbase);
	msgbase.close();
	if(result > 0)
		print(result + " corrupted messages headers found");
	return result;
}

function base_filename(fullname)
{
	var ext = file_getext(fullname);

	if(!ext)
		return fullname;

	return fullname.slice(0, -ext.length);
}

for(var i in argv) {
	switch(argv[i]) {
		case '-debug':
			debug = true;
			break;
		case '-scan':
			scan_only = true;
			break;
		default:
			open_and_scrub_base(base_filename(argv[i]));
			break;
	}
}
