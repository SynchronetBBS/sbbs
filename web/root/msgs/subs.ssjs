load("html_inc/msgslib.ssjs");

title="Message Subs in "+msg_area.grp[grp].description;

write_template("header.inc");

template.group=msg_area.grp[grp];
template.subs=msg_area.grp[grp].sub_list;

for(s in msg_area.grp[grp].sub_list) {
msgbase = new MsgBase(msg_area.grp[grp].sub_list[s].code);
    if(msgbase.open()) {
          msgs=msgbase.total_msgs;
          lastmsg=msgbase.last_msg;
          if(lastmsg>0) {
          lastdate=msgbase.get_msg_header(false,lastmsg);
          lastdate=parseInt(lastdate.date);
          lastdate=strftime("%m/%d/%y",lastdate);
          }
          else
          lastdate="Unknown";
          msgbase.close();
          template.subs[s].messages=msgs;
          template.subs[s].lastmsg=lastdate;
          
      }
}

write_template("msgs/subs.inc");
write_template("footer.inc");

msgs_done();
