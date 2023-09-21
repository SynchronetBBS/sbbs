// Delete msgs
// usage: [msg-base=mail] [hdr-field=from] "value"

load("sbbsdefs.js");

"use strict";

var base = new MsgBase(argv[0] || "mail");
var field = argv[1] || "from";
var value = String(argv[2]).toLowerCase();
if(!base.open())
	throw new Error("Can't open base: " + base.code);
var total_msgs = base.total_msgs;
var removed = 0;
for(i=0;i<total_msgs;i++) {
	hdr = base.get_msg_header(	/* by_offset:		*/	true,
								/* offset:			*/	i,
								/* expand_fields:	*/	false);
	printf("#%lu from: %-30s %08lx\r\n",hdr.number,hdr.from,hdr.attr);
	if(hdr && !(hdr.attr & MSG_DELETE) && String(hdr[field]).toLowerCase() == value) {
		hdr.attr |= MSG_DELETE;
		printf("Removing message #%lu\r\n",hdr.number);
		if(!base.put_msg_header(true,i,hdr))
			alert(base.last_error);
		else {
			removed++;
		}
	}
}

base.close();

print();
print(format("Removed %u msgs", removed));
