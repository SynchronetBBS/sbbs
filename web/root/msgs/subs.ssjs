load("html_inc/msgslib.ssjs");

template.title="Message Subs in Group: "+msg_area.grp[grp].description;

write_template("header.inc");

template.group=msg_area.grp[grp];
template.subs=msg_area.grp[grp].sub_list;

for(s in msg_area.grp[grp].sub_list) {
	msgbase = new MsgBase(msg_area.grp[grp].sub_list[s].code);
	if(msgbase.open()) {
		var lastdate="No Msgs";
        msgs=msgbase.total_msgs;
		if(msgs != undefined && msgs > 0) {
			lastdate=msgbase.get_msg_index(true,msgs-1);
			if(lastdate!=undefined && lastdate != null) {
				lastdate=lastdate.time;
				if(lastdate>0)
					lastdate=strftime("%m/%d/%y",lastdate);
			}
        }
        msgbase.close();
        template.subs[s].messages=msgs;
        template.subs[s].lastmsg=lastdate;
	}
}

write_template("msgs/subs.inc");
write_template("footer.inc");

msgs_done();
