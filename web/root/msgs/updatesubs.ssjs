/* $Id$ */

load("../web/lib/template.ssjs");
load("../web/lib/msgslib.ssjs");

template.title="New Scan Updated";

grp = http_request.query.msg_grp;

template.group=msg_area.grp[grp];
template.subs=new Array;

for(obj in http_request.query) {
      if(obj.substr(0,4)=='sub-') {
        sub = obj.slice(4);
        if(http_request.query[obj]==1) {
           msg_area.sub[sub].scan_cfg&= ~SCAN_CFG_NEW;
           msg_area.sub[sub].scan_cfg&= ~SCAN_CFG_YONLY;
        }           
        if(http_request.query[obj]==2) {
           msg_area.sub[sub].scan_cfg|= SCAN_CFG_NEW;
           msg_area.sub[sub].scan_cfg&= ~SCAN_CFG_YONLY;
        }
        if(http_request.query[obj]==3) {
           msg_area.sub[sub].scan_cfg&= ~SCAN_CFG_NEW;
           msg_area.sub[sub].scan_cfg|= SCAN_CFG_YONLY;
        }
   }
}

for(s in msg_area.grp[grp].sub_list) {
  ischecked = 1; /* New Message Scan Off */
  if(msg_area.grp[grp].sub_list[s].scan_cfg&SCAN_CFG_NEW)
    ischecked = 2;
  if(msg_area.grp[grp].sub_list[s].scan_cfg&SCAN_CFG_YONLY)
    ischecked = 3;
    var thissub=msg_area.grp[grp].sub_list[s];
    msgbase = new MsgBase(msg_area.grp[grp].sub_list[s].code);
    if(msgbase.open()) {
        var lastdate="N/A";
        msgs=msgbase.total_msgs;
        if(msgs != undefined && msgs > 0) {
            lastdate=msgbase.get_msg_index(true,msgs-1);
            if(lastdate!=undefined && lastdate != null) {
                lastdate=lastdate.time;
                if(lastdate>0)
                    lastdate=strftime("%b-%d-%y",lastdate);
            }
        }
        msgbase.close();
        thissub.messages=msgs;
        thissub.lastmsg=lastdate;
        thissub.ischecked=ischecked;
    }
    template.subs.push(thissub);
}

if(do_header)
	write_template("header.inc");
if(do_topnav)
	load("../web/lib/topnav_html.ssjs");
if(do_leftnav)
load("../web/lib/leftnav_html.ssjs");
if(do_rightnav)
	write_template("rightnav.inc");
write_template("msgs/updatesubs.inc");
if(do_footer)
	write_template("footer.inc");
