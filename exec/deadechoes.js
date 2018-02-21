var t = time();
var sub;
for(sub in msg_area.sub) {
	printf("%20s: ", sub);
	var msgbase = MsgBase(sub);
	if(!msgbase.open()) {
		alert(msgbase.last_error);
		continue;
	}
	if(!msgbase.total_msgs) {
		alert("No messages");
		msgbase.close();
		continue;
	}
	var idx = msgbase.get_msg_index(/* by offset: */true, msgbase.total_msgs-1, /* include_votes: */true);
	if(idx == null) {
		alert("Can't read index of last message");
		continue;
	}
	var diff = t - idx.time;
	var days = diff / (60*60*24);
	if(days > 360)
		print(format("Inactive (%-5u days): %s", days, msg_area.sub[sub].description));
	else
		print("active");
	msgbase.close();
}
		