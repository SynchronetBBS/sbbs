/* $Id$ */

load("../web/lib/msgslib.ssjs");

var ShowAll=(http_request.query.show_all_subs != undefined
		&& http_request.query.show_all_subs == 'Yes');

var new_query='';
for(key in http_request.query) {
	if(key != 'show_all_subs') {
		if(new_query.length>0)
			new_query+='&amp;';
		new_query+=encodeURIComponent(key);
		new_query+='=';
		new_query+=encodeURIComponent(http_request.query[key]);
	}
}
if(new_query.length>0)
	new_query+='&amp;';
if(user.security.restrictions&UFLAG_G)
	ShowAll=true;
else {
	new_query+='show_all_subs=';
	template.showall_toggle='<a href="'+http_request.virtual_path+'?'+new_query
	if(ShowAll)
		template.showall_toggle+='No">'+showall_subs_disable_html;
	else
		template.showall_toggle+='Yes">'+showall_subs_enable_html;
	template.showall_toggle+='</a>';
}
template.title="Message Subs in Group: "+msg_area.grp[grp].description;

if(do_header)
	write_template("header.inc");

template.group=msg_area.grp[grp];
template.subs=new Array;

for(s in msg_area.grp[grp].sub_list) {
	if(!ShowAll && !(msg_area.grp[grp].sub_list[s].scan_cfg&(SCAN_CFG_YONLY|SCAN_CFG_NEW)))
		continue;
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
					lastdate=strftime("%b-%d-%y %H:%M",lastdate);
			}
        }
        msgbase.close();
        thissub.messages=msgs;
        thissub.lastmsg=lastdate;
	}
	template.subs.push(thissub);
}

if(do_topnav)
	load(topnav_html);
if(do_leftnav)
load(leftnav_html);
if(do_rightnav)
	write_template("rightnav.inc");
write_template("msgs/subs.inc");
if(do_footer)
	write_template("footer.inc");

msgs_done();
