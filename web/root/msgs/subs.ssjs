load("html_inc/msgslib.ssjs");

title="Message Subs in "+msg_area.grp[grp].description;

write_template("header.inc");

template.group=msg_area.grp[grp];
template.subs=msg_area.grp[grp].sub_list;

for(s in msg_area.grp[grp].sub_list) {
msgbase = new MsgBase(msg_area.grp[grp].sub_list[s].code);
	if(msgbase.open()) {
		var lastdate="Unknown";
        msgs=msgbase.total_msgs;
		if(msgs != undefined && msgs > 0) {
        	lastmsg=msgbase.last_msg;
			if(lastmsg != undefined && lastmsg > 0) {
				lastdate=msgbase.get_msg_header(false,lastmsg);
				if(lastdate!=undefined && lastdate != null) {
					lastdate=new Date(lastdate.date);
					lastdate=lastdate.getTime()/1000;
					if(lastdate>0)
						lastdate=strftime("%m/%d/%y",lastdate);
				}
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
