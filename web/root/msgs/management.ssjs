load("html_inc/template.ssjs");
load("html_inc/msgslib.ssjs");

template.title="No action taken";
template.detail="No action taken";

if(msgbase.open!=undefined && msgbase.open()==false) {
	error(msgbase.last_error);
}

if(http_request.query.Action=="Delete Message(s)") {
	var hdr;
	var deleted=0;
	var errors=0;
	errorlist=new Array;

	if(sub!='mail' && !msg_area.grp_list[g].sub_list[s].is_operator) {
		errorlist.push("Only operators can delete messages!");
		errors++;
	}
	else {
		for(num in http_request.query.number) {
			var mnum=parseInt(http_request.query.number[num]);
			if(sub==mail && ((idx=get_msg_index(false,mnum))==null || idx.to!=user.number) {
				errors++;
				errorlist.push("Cannot delete message "+mnum);
			}
			else {
				if(msgbase.remove_msg(false,mnum))
					deleted++;
				else {
					errors++;
					errorlist.push(msgbase.last_error);
				}
			}
		}
	}
	template.title=deleted+" Messages Deleted";
	template.detail=deleted+" Messages Deleted ("+errors+" errors";
	if(errors)
		template.detail+=": "+errorlist;
	template.detail+=")";
}
template.backurl=http_request.header.Referer;

write_template("header.inc");
write_template("msgs/management.inc");
write_template("footer.inc");

msgs_done();
