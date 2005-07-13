load("../web/lib/template.ssjs");
load("../web/lib/msgslib.ssjs");

template.title="No action taken";
template.detail="No action taken";

if(msgbase.open!=undefined && msgbase.open()==false) {
    error(msgbase.last_error);
}

if(http_request.query.Action=="Delete") {
    var hdr;
    var deleted=0;
    var errors=0;
    errorlist=new Array;

    for(num in http_request.query.number) {
        var mnum=parseInt(http_request.query.number[num]);

        if(sub=='mail' && ((idx=msgbase.get_msg_index(false,mnum))==null || idx.to!=user.number)) {
            errors++;
            errorlist.push("Cannot delete message "+mnum);
            continue;
        }
        if(sub!='mail' && !msg_area.sub[sub].is_operator) {
            if(!msg_area.sub[sub].settings&SUB_DEL) {
                errorlist.push("Only operators can delete messages!");
                errors++;
                continue;
            }
            if(msg_area.sub[sub].settings&SUB_DELLAST) {
                if(msgbase.last_msg!=mnum) {
                    errorlist.push("You can only delete the last post!");
                    errors++;
                    continue;
                }
            }
            if((hdr=msgbase.get_msg_header(false,mnum))==null || hdr.from_ext!=user.number) {
                errorlist.push("You can only delete your own messages!");
                errors++;
                continue;
            }
        }

        if(msgbase.remove_msg(false,mnum))
            deleted++;
        else {
            errors++;
            errorlist.push(msgbase.last_error);
        }
    }
    template.title=deleted+" Messages Deleted";
    template.detail=deleted+" Messages Deleted ("+errors+" errors";
    if(errors)
        template.detail+=": "+errorlist;
    template.detail+=")";
    
}
template.sub_code=sub;

write_template("header.inc");
load("../web/lib/topnav_html.ssjs");
load("../web/lib/leftnav_html.ssjs");
write_template("msgs/management.inc");
write_template("footer.inc");

msgs_done();
